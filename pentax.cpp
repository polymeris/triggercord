extern "C"
{
    #include "pslr.h"
    #include "pslr_enum.h"
    bool debug = 1;
}

#include "pentax.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>


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

const std::string Camera::model()
{
    return pslr_camera_name(theHandle);
}

pslr_exposure_mode_t getExposureMode(bool aA, bool aS, bool aI)
{
    if  ((!aA) && (!aS) && (!aI))
	return PSLR_EXPOSURE_MODE_M;
    if ((!aA) && (!aS))
	return PSLR_EXPOSURE_MODE_TAV;
    if ((!aA))
	return PSLR_EXPOSURE_MODE_AV;
    if (( aA) && (!aS))
	return PSLR_EXPOSURE_MODE_TV;
    if (( aA) && ( aS) && (!aI))
	return PSLR_EXPOSURE_MODE_SV;
    return PSLR_EXPOSURE_MODE_P;
}

void Camera::updateExposureMode()
{
    updateStatus();
    pslr_exposure_mode_t m = getExposureMode(apertureIsAuto, shutterIsAuto, isoIsAuto);
    pslr_set_exposure_mode(theHandle, m);
    updateStatus();
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
    path = getpwuid(getuid())->pw_dir;
    imageNumber = 0;
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
    DPRINT("Focused.");
}

std::string Camera::shoot()
{
    pslr_shutter(theHandle);
    updateStatus();
    std::string fn = getFilename();
    while (!saveBuffer(fn))
	usleep(10000);
    deleteBuffer();
    DPRINT("Shot.");
    return lastFilename;
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
    if (a < 0)
    {
	if (!apertureIsAuto)
	{
	    apertureIsAuto = true;
	    updateExposureMode();
	}
	return;
    }

    apertureIsAuto = false;
    updateExposureMode();

    a = std::min(a, maximumAperture());
    a = std::max(a, minimumAperture());

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
    if (s < 0)
    {
	if (!shutterIsAuto)
	{
	    shutterIsAuto = true;
	    updateExposureMode();
	}
	return;
    }

    shutterIsAuto = false;
    updateExposureMode();

    s = std::min(s, maximumShutter());
    s = std::max(s, minimumShutter());

    pslr_rational_t rational;
    if (s < .3)
    {
	rational.nom = 1;
	rational.denom = int(1.0 / s);
    }
    else if (s < 2.)
    {
	rational.nom = 20;
	rational.denom = int(20.0 / s);
    }
    else
    {
	rational.nom = int(s);
	rational.denom = 1;
    }
    DPRINT("S: %i/%i", rational.nom, rational.denom);
    pslr_set_shutter(theHandle, rational);
}

void Camera::setIso(int i, int min, int max)
{
    if (i == -1)
    {
	if (!isoIsAuto)
	{
	    isoIsAuto = true;
	    updateExposureMode();
	}
	return;
    }

    isoIsAuto = false;
    updateExposureMode();

    i = std::min(i, (int)maximumIso());
    i = std::max(i, (int)minimumIso());
    
    if (min == -1)
	min = i;
    if (max == -1)
	max = i;

    if (min == -100)
	min = minimumAutoIso();
    if (max == -100)
	max = maximumAutoIso();
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
	pslr_set_image_format(theHandle, PSLR_IMAGE_FORMAT_RAW);
    else
	pslr_set_image_format(theHandle, PSLR_IMAGE_FORMAT_JPEG);
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

std::string Camera::fileDestination()
{
    return path;
}

std::string Camera::fileExtension()
{
    if (raw())
	return "dng";
    return "jpg";
}

bool Camera::raw()
{
    updateStatus();
    return theStatus.image_format != PSLR_IMAGE_FORMAT_JPEG;
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

int Camera::exposureMode()
{
    updateStatus();
    return theStatus.exposure_mode;
}

int Camera::autofocusMode()
{
    updateStatus();
    return theStatus.af_mode;
}

float Camera::minimumAutoIso()
{
    return theStatus.auto_iso_min;
}

float Camera::maximumAutoIso()
{
    return theStatus.auto_iso_max;
}

std::string Camera::getFilename()
{
    std::stringstream filename;
    do
    {
	filename.str(std::string());
	filename.clear();
	imageNumber++;
	filename << fileDestination() << "/tc" << imageNumber << "." << fileExtension();
    } while (std::ifstream(filename.str().c_str())); // if it exists, find another name
    return filename.str();
}

bool Camera::saveBuffer(const std::string & filename)
{
    std::ofstream output;
    unsigned char buf[65536];
    int bytes;
    DPRINT("Writing to %s.", filename.c_str());
    if (pslr_buffer_open(
	theHandle, 0,
	raw() ? PSLR_BUF_DNG : pslr_get_jpeg_buffer_type(theHandle, theStatus.jpeg_quality),
	theStatus.jpeg_resolution)
	    != PSLR_OK)
    {
	DPRINT("Failed to open camera buffer.");
	return false;
    }
    output.open(filename.c_str());
    
    DPRINT("Buffer length: %d.", pslr_buffer_get_size(theHandle));
    do {
        bytes = pslr_buffer_read(theHandle, buf, sizeof (buf));
        output.write((char *)buf, bytes);
    } while (bytes);
    output.close();

    lastFilename = filename;
    return true;
}

void Camera::deleteBuffer()
{
    pslr_delete_buffer(theHandle, 0);
}

float Camera::maximumAperture()
{
    return rationalAsFloat(theStatus.lens_max_aperture);
}

float Camera::minimumAperture()
{
    return rationalAsFloat(theStatus.lens_min_aperture);
}

float Camera::maximumShutter()
{
    return 30.0;
}

float Camera::minimumShutter()
{
    return 1.0 / pslr_get_model_fastest_shutter_speed(theHandle);
}

float Camera::maximumIso()
{
    return pslr_get_model_extended_iso_max(theHandle);
}
float Camera::minimumIso()
{
    return pslr_get_model_extended_iso_min(theHandle);
}

float Camera::maximumExposureCompensation()
{
    return 5;
}

float Camera::minimumExposureCompensation()
{
    return -5;
}

std::string Camera::exposureModeAbbreviation()
{
    switch (exposureMode())
    {
	case 2:
	case 3:
	case PSLR_EXPOSURE_MODE_GREEN:
	case PSLR_EXPOSURE_MODE_P: return "P";
	case PSLR_EXPOSURE_MODE_SV: return "Sv";
	case PSLR_EXPOSURE_MODE_AV_OFFAUTO:
	case PSLR_EXPOSURE_MODE_AV: return "Av";
	case PSLR_EXPOSURE_MODE_TV: return "Tv";
	case PSLR_EXPOSURE_MODE_TAV: return "TAv";
	case PSLR_EXPOSURE_MODE_M_OFFAUTO:
	case PSLR_EXPOSURE_MODE_M: return "M";
	case PSLR_EXPOSURE_MODE_X: return "X";
	case PSLR_EXPOSURE_MODE_B_OFFAUTO:
	case PSLR_EXPOSURE_MODE_B: return "B";
	default: return "?";
    }
}
