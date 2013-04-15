#ifndef PSLR_STRINGS_H
#define PSLR_STRINGS_H

const char* pslr_color_space_str[PSLR_COLOR_SPACE_MAX] = {
    "sRGB",
    "AdobeRGB"
};

const char* pslr_af_mode_str[PSLR_AF_MODE_MAX] = {
    "MF",
    "AF.S",
    "AF.C",
    "AF.A"
};

const char* pslr_ae_metering_str[PSLR_AE_METERING_MAX] = {
    "Multi",
    "Center",
    "Spot"
};

const char* pslr_flash_mode_str[PSLR_FLASH_MODE_MAX] = {
    "Manual",
    "Manual-RedEye",
    "Slow",
    "Slow-RedEye",
    "TrailingCurtain",
    "Auto",
    "Auto-RedEye",
    "TrailingCurtain", // maybe in manual mode??
    "Wireless"
};

const char* pslr_drive_mode_str[PSLR_DRIVE_MODE_MAX] = {
    "Single", // Bracketing also returns Single
    "Continuous-HI",
    "SelfTimer-12",
    "SelfTimer-2",
    "Remote",
    "Remote-3",
    "Continuous-LO"
};

const char*  pslr_af_point_sel_str[PSLR_AF_POINT_SEL_MAX] = {
    "Auto-5",
    "Select",
    "Spot",
    "Auto-11"
};

const char* pslr_jpeg_image_tone_str[PSLR_JPEG_IMAGE_TONE_MAX] = {
    "Natural",
    "Bright",
    "Portrait",
    "Landscape",
    "Vibrant",
    "Monochrome",
    "Muted",
    "ReversalFilm",
    "BleachBypass",
    "Radiant"
};

const char* pslr_white_balance_mode_str[PSLR_WHITE_BALANCE_MODE_MAX] = {
    "Auto",
    "Daylight",
    "Shade",
    "Cloudy",
    "Fluorescent_D",
    "Fluorescent_N",
    "Fluorescent_W",
    "Tungsten",
    "Flash",
    "Manual",
    "0x0A", // ??
    "0x0B", // ??
    "0x0C", // ?? exif: set color temp1
    "0x0D", // ?? exif: set color temp2
    "0xOE", // ?? exif: set color temp3
    "Fluorescent_L",
    "CTE"
};

const char* pslr_custom_ev_steps_str[PSLR_CUSTOM_EV_STEPS_MAX] = {
    "1/2",
    "1/3"
};

const char* pslr_raw_format_str[PSLR_RAW_FORMAT_MAX] = {
    "PEF",
    "DNG"
};

#endif
