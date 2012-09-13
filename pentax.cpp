extern "C"
{
    #include "pslr.h"
    #include "pslr_enum.h"
    bool debug = 1;
}

#include "pentax.h"
#include <cstdlib>

#define IS_AUTO(x) ((x) < 0)

#ifndef ANDROID
    #define LOGD(msg) printf("Debug: %s\n", msg)
    #define LOGI(msg) printf("%s\n", msg)
    #define LOGW(msg) printf("Warning: %s\n", msg)
    #define LOGE(msg) printf("Error: %s\n", msg)
#else
    #include <android/log.h>
    #define LOGD(msg) \
	__android_log_print(ANDROID_LOG_DEBUG, "Pentax", msg)
    #define LOGI(msg) \
	__android_log_print(ANDROID_LOG_INFO, "Pentax", msg)
    #define LOGW(msg) \
	__android_log_print(ANDROID_LOG_WARN, "Pentax", msg)
    #define LOGE(msg) \
	__android_log_print(ANDROID_LOG_ERROR, "Pentax", msg)
#endif

static std::string theModel = "none";
static char *theDevice = NULL;

static pslr_handle_t theHandle;
static Camera * theCamera = NULL;

pslr_rational_t rational;

void cleanup()
{
    if (theCamera)
	delete theCamera;
}

bool apertureIsAuto, shutterIsAuto, isoIsAuto;

const std::string & Camera::model()
{
    return theModel;
}

void Camera::updateExposureMode()
{
    bool aA = apertureIsAuto;
    bool aS = shutterIsAuto;
    bool aI = isoIsAuto;
    pslr_exposure_mode_t m;

    if      (!aA && !aS && !aI)
	m = PSLR_EXPOSURE_MODE_M;
    else if (!aA && !aS)
	m = PSLR_EXPOSURE_MODE_TAV;
    else if (!aA)
	m = PSLR_EXPOSURE_MODE_AV;
    else if ( aA && !aS)
	m = PSLR_EXPOSURE_MODE_TV;
    else if ( aA &&  aS && !aI)
	m = PSLR_EXPOSURE_MODE_SV;
    m = PSLR_EXPOSURE_MODE_P;
    pslr_set_exposure_mode(theHandle, m);
}

const Camera * camera()
{
    char * model = NULL;
    if(!theCamera && (theHandle = pslr_init(model, theDevice)))
    {
	atexit(cleanup);
	theCamera = new Camera();
	theModel = std::string(model);
    }

    if (!theCamera)
	LOGW("Camera not found.");
    else
    {
	LOGI("Found camera:");
	LOGI(model);
    }
    return theCamera;
}

Camera::Camera()
{
    pslr_connect(theHandle);
}

Camera::~Camera()
{
    pslr_disconnect(theHandle);
    pslr_shutdown(theHandle);
    theCamera = NULL;
}

void Camera::focus()
{
    pslr_focus(theHandle);
}

void Camera::shoot()
{
    pslr_shutter(theHandle);
}

void Camera::setExposure(float a, float s, int i)
{
    this->setAperture(a);
    this->setShutter(s);
    this->setIso(i);
}

void Camera::setMetering(int m, float ec)
{
    this->setExposureCompensation(ec);
}

void Camera::setAutofocus(int m, int pt)
{
    this->setAutofocusPoint(pt);
}

void Camera::setAperture(float a)
{
    if (IS_AUTO(a))
    {
	if (!apertureIsAuto)
	{
	    apertureIsAuto = true;
	    updateExposureMode();
	}
	return;
    }

    if (apertureIsAuto)
    {
	apertureIsAuto = false;
	updateExposureMode();
    }
    if (a >= 11.0)
    {
	rational.nom = int(a);
	rational.denom = 1;
    }
    else
    {
	rational.nom = int(a * 10);
	rational.denom = 10;
    }
    pslr_set_aperture(theHandle, rational);
}

void Camera::setShutter(float s)
{
    if (IS_AUTO(s))
    {
	if (!shutterIsAuto)
	{
	    shutterIsAuto = true;
	    updateExposureMode();
	}
	return;
    }

    if (shutterIsAuto)
    {
	shutterIsAuto = true;
	updateExposureMode();
    }
    if (s < 5)
    {
	rational.nom = 10;
	rational.denom = int(10.0 / s);
    }
    else
    {
	rational.nom = int(s);
	rational.denom = 1;
    }
    pslr_set_shutter(theHandle, rational);
}

void Camera::setIso(int i, int min, int max)
{
    if (IS_AUTO(i))
    {
	if (!isoIsAuto)
	{
	    isoIsAuto = true;
	    updateExposureMode();
	}
	return;
    }

    if (isoIsAuto)
    {
	isoIsAuto = false;
	updateExposureMode();
    }
    if (IS_AUTO(min))
	min = i;
    if (IS_AUTO(max))
	max = i;
    pslr_set_iso(theHandle, i, min, max);
}

void Camera::setExposureCompensation(float ec)
{
    rational.nom = int(ec * 10);
    rational.denom = 10;
    pslr_set_ec(theHandle, rational);
}

void Camera::setAutofocusPoint(int pt)
{
}

void Camera::setJpegAdjustments(int sat, int hue, int con, int sha)
{
    if (sat != KEEP)
	pslr_set_jpeg_saturation(theHandle, sat);
    if (hue != KEEP)
	pslr_set_jpeg_hue(theHandle, hue);
    if (con != KEEP)
	pslr_set_jpeg_contrast(theHandle, con);
    if (sha != KEEP)
	pslr_set_jpeg_sharpness(theHandle, sha);
}

float Camera::aperture()
{
    LOGW("Calling stub method.");
    return 4.2;
}

float Camera::shutter()
{
    LOGW("Calling stub method.");
    return .42;
}

float Camera::iso()
{
    LOGW("Calling stub method.");
    return 420;
}

float Camera::exposureCompensation()
{
    LOGW("Calling stub method.");
    return .42;
}

int Camera::focusPoint()
{
    LOGW("Calling stub method.");
    return 4;
}

int Camera::meteringMode()
{
    LOGW("Calling stub method.");
    return Camera::AUTO;
}

int Camera::autofocusMode()
{
    LOGW("Calling stub method.");
    return Camera::AUTO;
}
