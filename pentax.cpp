extern "C"
{
    #include "pslr.h"
    #include "pslr_enum.h"
    bool debug = 1;
}

#include "pentax.h"
#include <cmath>
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>


static char *theDevice = NULL;
static pslr_handle_t theHandle;
static pslr_status theStatus;
static Camera * theCamera = NULL;

void cleanup()
{
    if (theCamera)
	delete theCamera;
}

inline float roundToSignificantDigits(float f, int d)
{
  int p = f - log10(f);
  return floor(f * pow(10., p) + .5) / pow(10., p);
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
    return fromSixthStops(ts * 3);
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

pslr_rational_t asRationalExposureCompensation(const Stop & stop)
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
    requestedStopChanges[p] = s;
}

void Camera::set(const Parameter & p, const String & s)
{
    requestedStringChanges[p] = s;
}

Stop Camera::stop(const Parameter & p) const
{
    return currentStopValues.at(p);
}

std::string Camera::string(const Parameter & p) const
{
    return currentStringValues.at(p);
}

int Camera::minimum(const Parameter & p) const
{
    
}

int Camera::maximum(const Parameter & p) const
{
}

std::string Camera::accepptedString(const Parameter & p, int)
{
}

void Camera::applyChanges()
{
}

const std::string Camera::STRING_VALUE_PARAMETERS[] =
{
    "File Destination", "Flash Mode", "Drive Mode", "Autofocus Mode", "Autofocus Point",
    "Metering Mode", "Color Space"
};

const char* EXPOSURE_MODES[] = {
    "P", "Green", "?", "?", "?", "Tv", "Av", "?", "?", "M", "B", "Av", "M", "B", "TAv", "Sv", "X"
};

namespace xtern
{
    #include "pslr_strings.h"
}
#define STRING_IN_ARRAY(arr, idx) ((idx >= sizeof(arr)/sizeof(std::string))? "?" : arr[idx])

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
    return "?";
}

Stop retrieveStopValue(const Camera::Parameter & p)
{
    if (p == "Shutterspeed") return Stop::fromShutterspeed(asFloat(theStatus.current_aperture));
    if (p == "Aperture")     return Stop::fromAperture(asFloat(theStatus.current_aperture));
    if (p == "ISO")          return Stop::fromISO(theStatus.fixed_iso);
    if (p == "Exposure Compensation") return Stop::fromExposureCompensation(asFloat(theStatus.ec));
    if (p == "Flash Exposure Compensation")
	return Stop::fromExposureCompensation(theStatus.flash_exposure_compensation);
    return Stop::UNKNOWN;
}
    
int retrieveIntValue(const Camera::Parameter & p)
{
    //~ if (p == "Whitebalance Adjustment Magenta/Green") return 
    //~ if (p == "Whitebalance Adjustment Blue/Amber")    return
    if (p == "JPEG Quality")    return theStatus.jpeg_quality;
    if (p == "JPEG Resolution") return theStatus.jpeg_resolution;
    if (p == "JPEG Image Tone") return theStatus.jpeg_image_tone;
    if (p == "JPEG Sharpness")  return theStatus.jpeg_sharpness;
    if (p == "JPEG Contrast")   return theStatus.jpeg_contrast;
    if (p == "JPEG Saturation") return theStatus.jpeg_saturation;
    if (p == "JPEG Hue")        return theStatus.jpeg_hue;
    return Camera::UNKNOWN;
}

void Camera::updateValues()
{
    pslr_get_status(theHandle, &theStatus);
    for (int i = 0; i < sizeof(STRING_VALUE_PARAMETERS); i++)
    {
	const std::string & param = STRING_VALUE_PARAMETERS[i];
	currentStringValues[param] = retrieveStringValue(param);
    }
}

const Camera * camera()
{
    if(!theCamera && (theHandle = pslr_init(NULL, theDevice)))
    {
	atexit(cleanup);
	theCamera = new Camera();
    }

    if (!theCamera)
	DPRINT("Camera not found.");
    else
    {
	DPRINT("Found camera!");
	//~ DPRINT(theCamera->model().c_str());
    }
    return theCamera;
}

Camera::Camera()
{
    pslr_connect(theHandle);
    //~ path = getpwuid(getuid())->pw_dir;
}

Camera::~Camera()
{
    pslr_disconnect(theHandle);
    pslr_shutdown(theHandle);
    theCamera = NULL;
}
