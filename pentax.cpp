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

std::string Stop::getOneOverString() const
{
    std::stringstream ss;
    ss << "1/" << asShutterspeed();
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

void Camera::set(const Parameter & p, const Stop & s)
{
    LOCK_MUTEX;
    requestedStopChanges[p] = s;
    UNLOCK_MUTEX;
}

void Camera::set(const Parameter & p, int i)
{
    LOCK_MUTEX;
    set(p, Stop(i));
    UNLOCK_MUTEX;
}

void Camera::set(const Parameter & p, const String & s)
{
    LOCK_MUTEX;
    requestedStringChanges[p] = s;
    UNLOCK_MUTEX;
}

Stop Camera::get(const Parameter & p) const
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

int Camera::getMinimum(const Parameter & p) const
{
    /* TODO: stub */
    return 0;
}

int Camera::getMaximum(const Parameter & p) const
{
    /* TODO: stub */
    return 1;
}

const std::string Camera::STRING_VALUE_PARAMETERS[] =
{
    "File Destination", "Flash Mode", "Drive Mode", "Autofocus Mode", "Autofocus Point",
    "Metering Mode", "Color Space", "Exposure Mode", "JPEG Quality", "JPEG Resolution",
    "JPEG Image Tone"
};

const std::string Camera::STOP_VALUE_PARAMETERS[] =
{
    "Shutterspeed", "Aperture", "ISO", "Exposure Compensation", "Flash Exposure Compensation",
    "JPEG Sharpness", "JPEG Contrast", "JPEG Saturation", "JPEG Hue"
};

const char* EXPOSURE_MODES[] = {
    "P", "Green", "?", "?", "?", "Tv", "Av", "?", "?", "M", "B", "Av", "M", "B", "TAv", "Sv", "X"
};

namespace xtern
{
    #include "pslr_strings.h"
}
#define STRING_IN_ARRAY(arr, idx) ((idx >= sizeof(arr)/sizeof(char *))? "?" : arr[idx])

int findInArray(const char * arr[], const std::string & str)
{
    for (int i = 0; i < sizeof(arr) / sizeof(char *); i++)
	if (std::string(arr[i]) == str)
	    return i;
    return -1;
}

std::string Camera::getStringOption(const Parameter & p, int i)
{
    if (p == "Flash Mode")
	return STRING_IN_ARRAY(xtern::pslr_flash_mode_str, i);
    if (p == "Drive Mode")
	return STRING_IN_ARRAY(xtern::pslr_drive_mode_str, i);
    if (p == "Autofocus Mode")
    if (p == "Metering Mode")
	return STRING_IN_ARRAY(xtern::pslr_ae_metering_str, i);
    if (p == "Color Space")
	return STRING_IN_ARRAY(xtern::pslr_color_space_str, i);
    if (p == "Exposure Mode")
	return STRING_IN_ARRAY(EXPOSURE_MODES, i);
    return "?";
}

std::string retrieveStringValue(const Camera::Parameter & p)
{
    //~ if (p == "File Destination")  return 
    if (p == "Flash Mode")
	return STRING_IN_ARRAY(xtern::pslr_flash_mode_str, theStatus.flash_mode);
    if (p == "Drive Mode")
	return STRING_IN_ARRAY(xtern::pslr_drive_mode_str, theStatus.drive_mode);
    if (p == "Autofocus Mode")
	//~ return STRING_IN_ARRAY(x::pslr_af_mode_str, theStatus.af_mode);
    //~ if (p == "Autofocus Point")
	//~ return STRING_IN_ARRAY(
    if (p == "Metering Mode")
	return STRING_IN_ARRAY(xtern::pslr_ae_metering_str, theStatus.ae_metering_mode);
    if (p == "Color Space")
	return STRING_IN_ARRAY(xtern::pslr_color_space_str, theStatus.color_space);
    //~ if (p == "Image Format")
	//~ return STRING_IN_ARRAY(IMAGE_FORMATS, theStatus.image_format);
    //~ if (p == "Raw Format")
	//~ return STRING_IN_ARRAY(pslr_raw_format_str, theSt
    if (p == "Exposure Mode")
	return STRING_IN_ARRAY(EXPOSURE_MODES, theStatus.exposure_mode);
    //~ if (p == "Whitebalance Mode") return STRING_IN_ARRAY(pslr_white_balance_mode_str, theS
    //~ if (p == "JPEG Quality")    return theStatus.jpeg_quality;
    //~ if (p == "JPEG Resolution") return theStatus.jpeg_resolution;
    //~ if (p == "JPEG Image Tone") return theStatus.jpeg_image_tone;
    return "?";
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
    for (int i = 0; i < sizeof(STRING_VALUE_PARAMETERS) / sizeof(std::string); i++)
    {
	const std::string & param = STRING_VALUE_PARAMETERS[i];
	currentStringValues[param] = retrieveStringValue(param);
    }

    for (int i = 0; i < sizeof(STOP_VALUE_PARAMETERS) / sizeof(std::string); i++)
    {
	const std::string & param = STOP_VALUE_PARAMETERS[i];
	currentStopValues[param] = retrieveStopValue(param);
    }
    UNLOCK_MUTEX;
}

int sendChange(const Camera::Parameter & p, const std::string & s)
{
    int i = 0;
    if (p == "Flash Mode")
	return (i = findInArray(xtern::pslr_flash_mode_str, s) < 0 ? -2 :
	    pslr_set_flash_mode(theHandle, (pslr_flash_mode_t)i));
    if (p == "Drive Mode")
	return (i = findInArray(xtern::pslr_drive_mode_str, s) < 0 ? -2 :
	    pslr_set_drive_mode(theHandle, (pslr_drive_mode_t)i));
    if (p == "Autofocus Mode")
	return (i = findInArray(xtern::pslr_af_mode_str, s) < 0 ? -2 :
	    pslr_set_af_mode(theHandle, (pslr_af_mode_t)i));
    if (p == "Metering Mode")
	return (i = findInArray(xtern::pslr_ae_metering_str, s) < 0 ? -2 :
	    pslr_set_ae_metering_mode(theHandle, (pslr_ae_metering_t)i));
    if (p == "Color Space")
	return (i = findInArray(xtern::pslr_color_space_str, s) < 0 ? -2 :
	    pslr_set_color_space(theHandle, (pslr_color_space_t)i));
    if (p == "Exposure Mode")
	return (i = findInArray(EXPOSURE_MODES, s) < 0 ? -2 :
	    pslr_set_exposure_mode(theHandle, (pslr_exposure_mode_t)i));
    return -1;
}

int sendChange(const Camera::Parameter & p, const Stop & s)
{
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
	sendChange((*it).first, (*it).second);
    requestedStringChanges.clear();
    
    for (std::map<Parameter, Stop>::const_iterator it = requestedStopChanges.begin();
	it != requestedStopChanges.end();
	++it)
	sendChange((*it).first, (*it).second);
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

Camera::Camera()
{
    pslr_connect(theHandle);
    //~ path = getpwuid(getuid())->pw_dir;
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
