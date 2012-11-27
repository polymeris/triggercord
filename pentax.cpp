extern "C"
{
    #include "pslr.h"
    #include "pslr_enum.h"
    bool debug = 1;
}

#include "pentax.h"
#include <ctime>
#include <cstdlib>

#define IS_AUTO(x) ((x) < 0)

static char *theDevice = NULL;
static time_t theLastUpdateTime = -1;
static pslr_handle_t theHandle;
static pslr_status theStatus;
static Camera * theCamera = NULL;

void cleanup()
{
    if (theCamera)
	delete theCamera;
}

bool apertureIsAuto, shutterIsAuto, isoIsAuto;

const std::string & Camera::model()
{
    return std::string(pslr_camera_name(theHandle));
}

void Camera::updateExposureMode()
{
    bool aA = apertureIsAuto;
    bool aS = shutterIsAuto;
    bool aI = isoIsAuto;
    pslr_exposure_mode_t m;
    updateStatus();

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

void Camera::updateStatus()
{
    if (theLastUpdateTime - time(NULL))
    {
	pslr_get_status(theHandle, &theStatus);
	theLastUpdateTime = time(NULL);
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
	DPRINT(theCamera->model().c_str());
    }
    return theCamera;
}

Camera::Camera()
{
    pslr_connect(theHandle);
    updateStatus();
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

    pslr_rational_t rational;
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

    pslr_rational_t rational;
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
    pslr_rational_t rational;
    rational.nom = int(ec * 10);
    rational.denom = 10;
    pslr_set_ec(theHandle, rational);
}

void Camera::setAutofocusPoint(int pt)
{
    if (pt == KEEP)
	return;
    pslr_select_af_point(theHandle, pt);
}

void Camera::setRaw(bool enabled)
{
    if (enabled)
	pslr_set_image_format(theHandle, PSLR_IMAGE_FORMAT_RAW_PLUS);
    else
	pslr_set_image_format(theHandle, PSLR_IMAGE_FORMAT_JPEG);
}

void Camera::setFileDestination(std::string path)
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

float rationalAsFloat(pslr_rational_t rat)
{
    return float(rat.nom) / float(rat.denom);
}

float Camera::aperture()
{
    updateStatus();
    return rationalAsFloat(theStatus.current_aperture);
}

float Camera::shutter()
{
    updateStatus();
    return rationalAsFloat(theStatus.current_shutter_speed);    
}

float Camera::iso()
{
    updateStatus();
    return float(theStatus.current_iso);
}

float Camera::exposureCompensation()
{
    updateStatus();
    return rationalAsFloat(theStatus.ec);
}

int Camera::focusPoint()
{
    updateStatus();
    return theStatus.selected_af_point;
}

int Camera::meteringMode()
{
    updateStatus();
    return theStatus.exposure_mode;
}

int Camera::autofocusMode()
{
    updateStatus();
    return theStatus.af_mode;
}
