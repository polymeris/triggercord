extern "C"
{
    #include "pslr.h"
    #include "pslr_enum.h"
    bool debug = 1;
}

#include "pentax.h"
#include <pthread.h>
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

const static int MIN_UPDATE_INTERVAL = 2; // seconds

static char *theDevice = NULL;
pthread_t theUpdateThread;
pthread_mutex_t theMutex = PTHREAD_MUTEX_INITIALIZER;
bool theUpdateThreadRunning, theUpdateThreadExitFlag;
static pslr_handle_t theHandle;
static pslr_status theStatus;
static Camera * theCamera = NULL;

#define LOCK_MUTEX 	pthread_mutex_lock(&theMutex)
#define UNLOCK_MUTEX	pthread_mutex_unlock(&theMutex);

inline float roundToSignificantDigits(float f, int d)
{
    float p = pow(10.0, d - ceil(log10(fabs(f))));
    return round(f * p) / p; 
}

inline int roundFloat(float f)
{
    return f > 0.0 ? floor(f + 0.5) : ceil(f - 0.5);
}

inline double log2(double x)
{
    return log(x) / log(2.);
}

float rationalAsFloat(pslr_rational_t r)
{
    return float(r.nom) / float(r.denom);
}

const Stop Stop::UNKNOWN = Stop(-1000);
const Stop Stop::AUTO = Stop(-2000);
const Stop Stop::HALF = Stop::fromHalfStops(1);
const Stop Stop::THIRD = Stop::fromThirdStops(1);

Stop::Stop(int stops) : sixthStops(6 * stops)
{
}

Stop Stop::fromHalfStops(int hs)
{
    return fromSixthStops(hs * 3);
}

Stop Stop::fromThirdStops(int ts)
{
    return fromSixthStops(ts * 2);
}

Stop Stop::fromExposureSteps(int steps)
{
    return fromSixthStops(steps * 3);
}

Stop Stop::fromSixthStops(int ss)
{
    Stop ret;
    ret.sixthStops = ss;
    return ret;
}

Stop Stop::fromAperture(float a)
{
    return fromSixthStops(roundFloat(log2(a) * 12.));
}

Stop Stop::fromShuttertime(float t)
{
    return fromSixthStops(roundFloat(log2(t) * 6.));
}

Stop Stop::fromShutterspeed(float s)
{
    return fromSixthStops(roundFloat(log2(s) * -6.));
}

Stop Stop::fromISO(int i)
{
    return fromSixthStops(roundFloat(log2(float(i) / 100.f) * 6.));
}

Stop Stop::fromExposureCompensation(float ec)
{
    return fromSixthStops(roundFloat(ec * 6.));
}

Stop & Stop::operator+=(const Stop & other)
{
    sixthStops += other.sixthStops;
    return *this;
}

Stop & Stop::operator-=(const Stop & other)
{
    sixthStops -= other.sixthStops;
    return *this;
}

const Stop Stop::operator+(const Stop & other) const
{
    Stop ret = *this;
    ret += other;
    return ret;
}

const Stop Stop::operator-(const Stop & other) const
{
    Stop ret = *this;
    ret -= other;
    return ret;
}

const Stop Stop::operator*(int x) const
{
    return fromSixthStops(sixthStops * x);
}

bool Stop::operator!=(const Stop & other) const
{
    return this->sixthStops != other.sixthStops;
}

bool Stop::operator==(const Stop & other) const
{
    return this->sixthStops == other.sixthStops;
}

const Stop Stop::plus(const Stop & other) const
{
    return *this + other;
}

const Stop Stop::minus(const Stop & other) const
{
    return *this - other;
}

const Stop Stop::times(int x) const
{
    return *this * x;
}

float Stop::asAperture() const
{
    return roundToSignificantDigits(powf(2., (float)sixthStops / 12.), 2);
}

float Stop::asShuttertime() const
{
    return roundToSignificantDigits(powf(2., (float)sixthStops / 6.), 2);
}

float Stop::asShutterspeed() const
{
    return roundToSignificantDigits(powf(2., (float)sixthStops / -6.), 2);
}

int Stop::asISO() const
{
    return (int)roundToSignificantDigits(100. * powf(2., (float)sixthStops / 6.), 2);
}

float Stop::asExposureCompensation() const
{
    return roundToSignificantDigits((float)sixthStops / 6., 3);
}

int Stop::asInt() const
{
    return roundFloat(asExposureCompensation());
}

int Stop::asExposureSteps() const
{
    return sixthStops / 3.;
}

std::string Stop::getPrettyString() const
{
    static const std::string SIXTHS_FRACTIONS[] =
	{ "", "1/6", "1/3", "1/2", "2/3", "5/6" };
    int wholes = sixthStops / 6;
    std::stringstream ss;
    if (sixthStops > -6 && sixthStops < 0)
	ss << "-";
    else if (sixthStops >= 6)
	ss << wholes << " ";
    ss << SIXTHS_FRACTIONS[abs(sixthStops - 6 * wholes)];
    return ss.str();
}

std::string Stop::getSecondsString() const
{
    std::stringstream ss;
    ss << asShuttertime() << "\"";
    return ss.str();
}

std::string Stop::getOneOverString(bool denomOnly) const
{
    std::stringstream ss;
    ss << (denomOnly ? "" : "1/") << asShutterspeed();
    return ss.str();
}

std::string Stop::getApertureString() const
{
    std::stringstream ss;
    ss << asAperture();
    return ss.str();
}

std::string Stop::getISOString() const
{
    std::stringstream ss;
    ss << asISO();
    return ss.str();
}

pslr_rational_t stopAsRationalAperture(const Stop & stop)
{
    pslr_rational_t r;
    float a = stop.asAperture();
    if (a > 10.)
    {
	r.nom = a;
	r.denom = 1;
    }
    else
    {
	r.nom = 10. * a;
	r.denom = 10;
    }
    return r;
}

pslr_rational_t stopAsRationalShuttertime(const Stop & stop)
{
    pslr_rational_t r;
    float s = stop.asShuttertime();
    if (s < .3)
    {
	r.nom = 1;
	r.denom = 1.0 / s;
    }
    else if (s < 2.)
    {
	r.nom = 10;
	r.denom = 10.0 / s;
    }
    else
    {
	r.nom = int(s);
	r.denom = 1;
    }
    return r;
}

pslr_rational_t stopAsRationalExposureCompensation(const Stop & stop)
{
    pslr_rational_t r;
    r.nom = stop.asExposureCompensation() * 10;
    r.denom = 10;
    return r;
}

float asFloat(const pslr_rational_t & r)
{
    return float(r.nom) / float(r.denom);
}

void Camera::setStop(const Parameter & p, const Stop & s)
{
    if (getStop(p) != s)
    {
	LOCK_MUTEX;
	requestedStopChanges[p] = s;
	UNLOCK_MUTEX;
    }
}

void Camera::setString(const Parameter & p, const String & s)
{
    if (p == "File Destination")
    {
	setFileDestination(s);
	return;
    }
    
    if (getString(p) != s)
    {
	LOCK_MUTEX;
	requestedStringChanges[p] = s;
	UNLOCK_MUTEX;
    }
}

void Camera::setStopByIndex(const Parameter & p, int i)
{
    setStop(p, getStopOption(p, i));
}

void Camera::setStringByIndex(const Parameter & p, int i)
{
    setString(p, getStringOption(p, i));
}

Stop Camera::getStop(const Parameter & p) const
{
    LOCK_MUTEX;
    std::map<Parameter, Stop>::const_iterator it = currentStopValues.find(p);
    UNLOCK_MUTEX;
    if (it == currentStopValues.end())
	return Stop::UNKNOWN;
    return it->second;
}

std::string Camera::getString(const Parameter & p) const
{
    LOCK_MUTEX;
    std::map<Parameter, std::string>::const_iterator it = currentStringValues.find(p);
    UNLOCK_MUTEX;
    if (it == currentStringValues.end())
	return "?";
    return it->second;
}

Stop Camera::getMinimum(const Parameter & p) const
{
    if (p == "Shutterspeed")
	return Stop::fromShuttertime(rationalAsFloat(theStatus.max_shutter_speed));
    if (p == "Aperture")
	return Stop::fromAperture(rationalAsFloat(theStatus.lens_min_aperture));
    if (p == "ISO")
	return Stop::fromISO(pslr_get_model_extended_iso_min(theHandle));
    if (p == "Exposure Compensation")
	return Stop::fromExposureCompensation(-5);
    if (p == "Flash Exposure Compensation")
	return -3;
    if (p == "JPEG Sharpness" || p == "JPEG Contrast" ||p == "JPEG Saturation" || p == "JPEG Hue")
	return -pslr_get_model_jpeg_property_levels(theHandle) / 2;
    return Stop();
}

Stop Camera::getMaximum(const Parameter & p) const
{
    if (p == "Shutterspeed")
	return Stop::fromShuttertime(30).asExposureSteps();
    if (p == "Aperture")
	return Stop::fromAperture(rationalAsFloat(theStatus.lens_max_aperture));
    if (p == "ISO")
	return Stop::fromISO(pslr_get_model_extended_iso_max(theHandle));
    if (p == "Exposure Compensation")
	return Stop::fromExposureCompensation(5);
    if (p == "Flash Exposure Compensation")
	return 3;
    if (p == "JPEG Sharpness" || p == "JPEG Contrast" ||p == "JPEG Saturation" || p == "JPEG Hue")
	return pslr_get_model_jpeg_property_levels(theHandle) / 2;
    return Stop();
}

typedef int (*Setter)(pslr_handle_t, int);

struct StringParameter
{
    std::string name;
    uint32_t * statusField;
    Setter setter;
    const char ** options;
    int numOptions;
};

namespace xtern
{
    #include "pslr_strings.h"
}

const char* EXPOSURE_MODES[] = {
    "P", "Green", "?", "?", "?", "Tv", "Av", "?", "?", "M", "B", "Av", "M", "B", "TAv", "Sv", "X"
};

const StringParameter STRING_PARAMETERS[] =
{
    {"File Destination", NULL, NULL, NULL },
    {"Flash Mode", &theStatus.flash_mode, (Setter)&pslr_set_flash_mode,
	xtern::pslr_flash_mode_str, PSLR_FLASH_MODE_MAX},
    {"Drive Mode", &theStatus.drive_mode, (Setter)&pslr_set_drive_mode,
	xtern::pslr_drive_mode_str, PSLR_DRIVE_MODE_MAX},
    {"Autofocus Mode", &theStatus.af_mode, (Setter)&pslr_set_af_mode,
	xtern::pslr_af_mode_str, PSLR_AF_MODE_MAX},
    {"Metering Mode", &theStatus.ae_metering_mode, (Setter)&pslr_set_ae_metering_mode,
	xtern::pslr_ae_metering_str, PSLR_AE_METERING_MAX},
    {"Color Space", &theStatus.color_space, (Setter)&pslr_set_color_space,
	xtern::pslr_color_space_str, PSLR_COLOR_SPACE_MAX},
    {"Exposure Mode", &theStatus.exposure_mode, NULL,
	EXPOSURE_MODES, PSLR_EXPOSURE_MODE_MAX}
};

int findStringParameter(const std::string & name)
{
    for (unsigned int i = 0; i < sizeof(STRING_PARAMETERS) / sizeof(StringParameter); i++)
    {
	if (name == STRING_PARAMETERS[i].name)
	    return i;
    }
    return -1;
}

int getIndexOfOption(StringParameter p, const std::string & str)
{
    for (int i = 0; i < p.numOptions; i++)
	if (std::string(p.options[i]) == str)
	    return i;
    return -1;
}

std::string getOptionByIndex(StringParameter p, int idx)
{
    if (idx > p.numOptions)
	return "?";
    return p.options[idx];
}

const std::string STOP_VALUE_PARAMETERS[] =
{
    "Shutterspeed", "Aperture", "ISO", "Exposure Compensation", "Flash Exposure Compensation",
    "JPEG Sharpness", "JPEG Contrast", "JPEG Saturation", "JPEG Hue"
};

std::string Camera::getStringOption(const Parameter & p, int i) const
{
    int j = findStringParameter(p);
    if (j < 0)
	return "?";
    return getOptionByIndex(STRING_PARAMETERS[j], i);
}

Stop Camera::getStopOption(const Parameter & p, int i) const
{
    if (p == "Shutterspeed" || p == "Aperture" || p == "ISO")
	return (i == 0? Stop::AUTO : getMinimum(p) + Stop::fromExposureSteps(i - 1));
    return getMinimum(p) + Stop(i);
}

int Camera::getStringCount(const Parameter & p) const
{
    int i = findStringParameter(p);
    if (i < 0)
	return 0;
    return STRING_PARAMETERS[i].numOptions;
}

int Camera::getStopCount(const Parameter & p) const
{
    return (getMaximum(p) - getMinimum(p)).asExposureSteps();
}

std::string Camera::getStopOptionAsString(const Parameter & p, int i) const
{
    Stop s = getStopOption(p, i);
    if (s == Stop::AUTO)
	return "AUTO";

    if (p == "Shutterspeed")
    {
	if  (s.asShuttertime() < .3)
	    return s.getOneOverString(true);
	else
	    return s.getSecondsString();

    }

    if (p == "Aperture")
	return s.getApertureString();

    if (p == "ISO")
	return s.getISOString();

    return s.getPrettyString();
}

std::string retrieveStringValue(const Camera::Parameter & p)
{
    int i = findStringParameter(p);
    if (i < 0 || (!STRING_PARAMETERS[i].options) || (!STRING_PARAMETERS[i].statusField))
	return "?";
    int idx = *STRING_PARAMETERS[i].statusField;
    return getOptionByIndex(STRING_PARAMETERS[i], idx);
}

Stop retrieveStopValue(const Camera::Parameter & p)
{
    if (p == "Shutterspeed")  return Stop::fromShutterspeed(1. / asFloat(theStatus.current_aperture));
    if (p == "Aperture")     return Stop::fromAperture(asFloat(theStatus.current_aperture));
    if (p == "ISO")          return Stop::fromISO(theStatus.fixed_iso);
    if (p == "Exposure Compensation") return Stop::fromExposureCompensation(asFloat(theStatus.ec));
    if (p == "Flash Exposure Compensation")
	return Stop::fromExposureCompensation(theStatus.flash_exposure_compensation);
    if (p == "JPEG Sharpness")  return theStatus.jpeg_sharpness;
    if (p == "JPEG Contrast")   return theStatus.jpeg_contrast;
    if (p == "JPEG Saturation") return theStatus.jpeg_saturation;
    if (p == "JPEG Hue")        return theStatus.jpeg_hue;
    //~ if (p == "Whitebalance Adjustment Magenta/Green") return 
    //~ if (p == "Whitebalance Adjustment Blue/Amber")    return
    return Stop::UNKNOWN;
}

void Camera::updateValues()
{
    LOCK_MUTEX;
    pslr_get_status(theHandle, &theStatus);
    for (unsigned int i = 0; i < sizeof(STRING_PARAMETERS) / sizeof(StringParameter); i++)
    {
	const std::string & param = STRING_PARAMETERS[i].name;
	currentStringValues[param] = retrieveStringValue(param);
    }

    for (unsigned int i = 0; i < sizeof(STOP_VALUE_PARAMETERS) / sizeof(std::string); i++)
    {
	const std::string & param = STOP_VALUE_PARAMETERS[i];
	currentStopValues[param] = retrieveStopValue(param);
    }
    UNLOCK_MUTEX;
}

int sendChange(const Camera::Parameter & p, const std::string & s)
{
    DPRINT("Sending string change %s: %s\n", p.c_str(), s.c_str());
    int i = findStringParameter(p);
    if (i < 0 || (!STRING_PARAMETERS[i].options) || !(STRING_PARAMETERS[i].setter))
	return -1;
    int j = getIndexOfOption(STRING_PARAMETERS[i], s);
    DPRINT("param option j = %i", j);
    if (j < 0)
	return -1;
    return STRING_PARAMETERS[i].setter(theHandle, j);
}

int sendChange(const Camera::Parameter & p, const Stop & s)
{
    DPRINT("Sending stop change %s: %s\n", p.c_str(), s.getPrettyString().c_str());
    if (p == "Shutterspeed")
	return pslr_set_shutter(theHandle, stopAsRationalShuttertime(s));
    if (p == "Aperture")
	return pslr_set_aperture(theHandle, stopAsRationalAperture(s));
    if (p == "ISO")
	return pslr_set_iso(theHandle, s.asISO(), s.asISO(), s.asISO());
    if (p == "Exposure Compensation")
	return pslr_set_ec(theHandle, stopAsRationalExposureCompensation(s));
    if (p == "Flash Exposure Compensation")
	return pslr_set_flash_exposure_compensation(theHandle,
	    stopAsRationalExposureCompensation(s));
    if (p == "JPEG Sharpness")  return pslr_set_jpeg_sharpness(theHandle, s.asInt());
    if (p == "JPEG Contrast")   return pslr_set_jpeg_contrast(theHandle, s.asInt());
    if (p == "JPEG Saturation") return pslr_set_jpeg_saturation(theHandle, s.asInt());
    if (p == "JPEG Hue")        return pslr_set_jpeg_hue(theHandle, s.asInt());
    return -1;
}

void Camera::applyChanges()
{
    LOCK_MUTEX;
    for (std::map<Parameter, String>::const_iterator it = requestedStringChanges.begin();
	it != requestedStringChanges.end();
	++it)
	sendChange(it->first, it->second);
    requestedStringChanges.clear();
    UNLOCK_MUTEX;

    LOCK_MUTEX;
    for (std::map<Parameter, Stop>::const_iterator it = requestedStopChanges.begin();
	it != requestedStopChanges.end();
	++it)
	sendChange(it->first, it->second);
    requestedStopChanges.clear();
    UNLOCK_MUTEX;
}

void * updateLoop(void * ms)
{
    long t = (long)ms;
    while (true)
    {
	theCamera->applyChanges();
	theCamera->updateValues();
	LOCK_MUTEX;
	if (theUpdateThreadExitFlag)
	{
	    theUpdateThreadRunning = false;
	    UNLOCK_MUTEX;
	    break;
	}
	UNLOCK_MUTEX;
	usleep(1000 * t);
    }
    pthread_exit(NULL);
}

void Camera::startUpdating(long ms)
{
    if (ms != 0 && !theUpdateThreadRunning)
    {
	theUpdateThreadRunning = true;
	theUpdateThreadExitFlag = false;
	pthread_create(&theUpdateThread, NULL, updateLoop, (void *)ms);
    }
}

void Camera::stopUpdating()
{
    LOCK_MUTEX;
    theUpdateThreadExitFlag = true;
    UNLOCK_MUTEX;
}

bool Camera::setFileDestination(std::string path)
{
    if (access(path.c_str(), F_OK) != 0)
	return false;
    struct stat status;
    stat(path.c_str(), &status);
    if (!S_ISDIR(status.st_mode))
	return false;
    this->path = path;
    return true;
}

std::string Camera::getFileDestination() const
{
    return path;
}

std::string Camera::getFileExtension()
{
    if (getString("Image Format") == "RAW")
	return "dng";
    return "jpg";
}

std::string Camera::getFilename()
{
    std::stringstream filename;
    do
    {
	filename.str(std::string());
	filename.clear();
	imageNumber++;
	filename << path << "/tc" << imageNumber << "." << getFileExtension();
    } while (std::ifstream(filename.str().c_str())); // if it exists, find another name
    return filename.str();
}

bool Camera::saveBuffer(const std::string & filename)
{
    std::ofstream output;
    unsigned char buf[65536];
    int bytes;

    LOCK_MUTEX;
    DPRINT("Writing to %s.", filename.c_str());
    if (pslr_buffer_open(
	theHandle, 0,
	getString("Image Format") == "RAW" ?
	PSLR_BUF_DNG : pslr_get_jpeg_buffer_type(theHandle, theStatus.jpeg_quality),
	theStatus.jpeg_resolution)
	!= PSLR_OK)
    {
	DPRINT("Failed to open camera buffer.");
	UNLOCK_MUTEX;
	return false;
    }
    DPRINT("Buffer length: %d.", pslr_buffer_get_size(theHandle));
    
    output.open(filename.c_str());
    
    do {
        bytes = pslr_buffer_read(theHandle, buf, sizeof (buf));
        output.write((char *)buf, bytes);
    } while (bytes);
    pslr_buffer_close(theHandle);

    output.close();
    UNLOCK_MUTEX;

    lastFilename = filename;
    return true;
}

void Camera::deleteBuffer()
{
    LOCK_MUTEX;
    pslr_delete_buffer(theHandle, 0);
    UNLOCK_MUTEX;
}

void Camera::focus()
{
    LOCK_MUTEX;
    pslr_focus(theHandle);
    UNLOCK_MUTEX;
    DPRINT("Focused.");
}

std::string Camera::shoot()
{
    if (pslr_shutter(theHandle) != PSLR_OK)
    {
	DPRINT("Did not shoot.");
	return "";
    };
    std::string fn = getFilename();
    while (!saveBuffer(fn))
	usleep(10000);
    deleteBuffer();
    DPRINT("Shot.");
    return lastFilename;
}

Camera::Camera()
{
    pslr_connect(theHandle);
    path = getpwuid(getuid())->pw_dir;
    imageNumber = 0;
    updateValues();
}

Camera::~Camera()
{
    pthread_join(theUpdateThread, NULL);
    pslr_disconnect(theHandle);
    pslr_shutdown(theHandle);
    theCamera = NULL;
    pthread_exit(NULL);
}

void cleanup()
{
    if (theCamera)
	delete theCamera;
}

const Camera * camera()
{
    if(!theCamera && (theHandle = pslr_init(NULL, theDevice)))
    {
	atexit(cleanup);
	theCamera = new Camera();
    }
    return theCamera;
}
