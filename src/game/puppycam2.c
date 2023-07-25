///Puppycam 2.2 by Fazana

#include <PR/ultratypes.h>
#include <PR/gbi.h>
#include "sm64.h"
#include "types.h"
#include "level_update.h"
#include "puppycam2.h"
#include "audio/external.h"
#include "audio/data.h"
#include "game_init.h"
#include "engine/math_util.h"
#include "print.h"
#include "engine/surface_collision.h"
#include "engine/surface_load.h"
#include "include/text_strings.h"
#include "segment2.h"
#include "ingame_menu.h"
#include "memory.h"
#include "object_list_processor.h"
#include "object_helpers.h"
#include "behavior_data.h"
#include "save_file.h"
#include "mario.h"
#include "puppyprint.h"
#include "debug_box.h"
#include "main.h"

#ifdef PUPPYCAM

static inline float smooth(float x) {
    x = CLAMP(x, 0, 1);
    return sqr(x) * (3.f - 2.f * x);
}

static inline float softClamp(float x, float a, float b) {
    return smooth((2.f / 3.f) * (x - a) / (b - a) + (1.f / 6.f)) * (b - a) + a;
}

#define DECELERATION 0.66f
#define DEADZONE 20
#define SCRIPT_MEMORY_POOL 0x1000

struct gPuppyStruct gPuppyCam[NUM_PLAYERS];
struct sPuppyVolume *sPuppyVolumeStack[MAX_PUPPYCAM_VOLUMES];
s16 sFloorHeight  = 0;
u8  gPCOptionOpen = 0;
s8  gPCOptionSelected = 0;
s32 gPCOptionTimer = 0;
u8  gPCOptionIndex = 0;
u8  gPCOptionScroll = 0;
u16 gPuppyVolumeCount = 0;
struct MemoryPool *gPuppyMemoryPool;
s32 gPuppyError = 0;

#if defined(VERSION_EU)
static unsigned char  gPCOptionStringsFR[][64] = {{NC_ANALOGUE_FR}, {NC_CAMX_FR}, {NC_CAMY_FR}, {NC_INVERTX_FR}, {NC_INVERTY_FR}, {NC_CAMC_FR}, {NC_SCHEME_FR}, {NC_WIDE_FR}, {OPTION_LANGUAGE_FR}};
static unsigned char  gPCOptionStringsDE[][64] = {{NC_ANALOGUE_DE}, {NC_CAMX_DE}, {NC_CAMY_DE}, {NC_INVERTX_DE}, {NC_INVERTY_DE}, {NC_CAMC_DE}, {NC_SCHEME_DE}, {NC_WIDE_DE}, {OPTION_LANGUAGE_DE}};
static unsigned char  gPCFlagStringsFR[][64] = {{OPTION_DISABLED_FR}, {OPTION_ENABLED_FR}, {OPTION_SCHEME1_FR}, {OPTION_SCHEME2_FR}, {OPTION_SCHEME3_FR}, {TEXT_ENGLISH}, {TEXT_FRENCH}, {TEXT_GERMAN},};
static unsigned char  gPCFlagStringsDE[][64] = {{OPTION_DISABLED_DE}, {OPTION_ENABLED_DE}, {OPTION_SCHEME1_DE}, {OPTION_SCHEME2_DE}, {OPTION_SCHEME3_DE}, {TEXT_ENGLISH}, {TEXT_FRENCH}, {TEXT_GERMAN},};
static unsigned char  gPCToggleStringsFR[][64] = {{NC_BUTTON_FR}, {NC_BUTTON2_FR}, {NC_OPTION_FR}, {NC_HIGHLIGHT_L}, {NC_HIGHLIGHT_R},};
static unsigned char  gPCToggleStringsDE[][64] = {{NC_BUTTON_DE}, {NC_BUTTON2_DE}, {NC_OPTION_DE}, {NC_HIGHLIGHT_L}, {NC_HIGHLIGHT_R},};
#endif
static unsigned char  gPCOptionStringsEN[][64] = {{NC_ANALOGUE_EN}, {NC_CAMX_EN}, {NC_CAMY_EN}, {NC_INVERTX_EN}, {NC_INVERTY_EN}, {NC_CAMC_EN}, {NC_SCHEME_EN}, {NC_WIDE_EN}, {OPTION_LANGUAGE_EN}};
static unsigned char  gPCFlagStringsEN[][64] = {{OPTION_DISABLED_EN}, {OPTION_ENABLED_EN}, {OPTION_SCHEME1_EN}, {OPTION_SCHEME2_EN}, {OPTION_SCHEME3_EN}, {TEXT_ENGLISH}, {TEXT_FRENCH}, {TEXT_GERMAN},};
static unsigned char  gPCToggleStringsEN[][64] = {{NC_BUTTON_EN}, {NC_BUTTON2_EN}, {NC_OPTION_EN}, {NC_HIGHLIGHT_L}, {NC_HIGHLIGHT_R},};


#define OPT 32 //Just a temp thing

static unsigned char  (*gPCOptionStringsPtr)[OPT][64] = (unsigned char (*)[OPT][64])&gPCOptionStringsEN;
static unsigned char  (*gPCFlagStringsPtr  )[OPT][64] = (unsigned char (*)[OPT][64])&gPCFlagStringsEN;
static unsigned char  (*gPCToggleStringsPtr)[OPT][64] = (unsigned char (*)[OPT][64])&gPCToggleStringsEN;


struct gPCOptionStruct
{
    u8 gPCOptionName; //This is the position in the newcam_options text array. It doesn't have to directly correlate with its position in the struct
    s16 *gPCOptionVar; //This is the value that the option is going to directly affect.
    u8 gPCOptionStart; //This is where the text array will start. Set it to 255 to have it be ignored.
    s32 gPCOptionMin; //The minimum value of the option.
    s32 gPCOptionMax; //The maximum value of the option.
};

static const struct gPCOptionStruct gPCOptions[] = { //If the min and max are 0 and 1, then the value text is used, otherwise it's ignored.
#ifdef WIDE
    {/*Option Name*/ 7, /*Option Variable*/ &gConfig.widescreen,       /*Option Value Text Start*/ 0, /*Option Minimum*/ FALSE, /*Option Maximum*/ TRUE},
#endif
#if MULTILANG
    {/*Option Name*/ 8, /*Option Variable*/ &gInGameLanguage,       /*Option Value Text Start*/ 4, /*Option Minimum*/ 1, /*Option Maximum*/ 3},
#endif
    {/*Option Name*/ 0, /*Option Variable*/ &gPuppyCam[0].options.analogue,       /*Option Value Text Start*/ 0, /*Option Minimum*/ FALSE, /*Option Maximum*/ TRUE},
    {/*Option Name*/ 6, /*Option Variable*/ &gPuppyCam[0].options.inputType,       /*Option Value Text Start*/ 2, /*Option Minimum*/ 0, /*Option Maximum*/ 2},
    {/*Option Name*/ 1, /*Option Variable*/ &gPuppyCam[0].options.sensitivityX,   /*Option Value Text Start*/ 255, /*Option Minimum*/ 10, /*Option Maximum*/ 500},
    {/*Option Name*/ 2, /*Option Variable*/ &gPuppyCam[0].options.sensitivityY,   /*Option Value Text Start*/ 255, /*Option Minimum*/ 10, /*Option Maximum*/ 500},
    {/*Option Name*/ 3, /*Option Variable*/ &gPuppyCam[0].options.invertX,        /*Option Value Text Start*/ 0, /*Option Minimum*/ FALSE, /*Option Maximum*/ TRUE},
    {/*Option Name*/ 4, /*Option Variable*/ &gPuppyCam[0].options.invertY,        /*Option Value Text Start*/ 0, /*Option Minimum*/ FALSE, /*Option Maximum*/ TRUE},
    {/*Option Name*/ 5, /*Option Variable*/ &gPuppyCam[0].options.turnAggression, /*Option Value Text Start*/ 255, /*Option Minimum*/ 0, /*Option Maximum*/ 100},
};

u8 gPCOptionCap = sizeof(gPCOptions) / sizeof(struct gPCOptionStruct); //How many options there are in newcam_uptions.

s16 LENSIN(s16 length, s16 direction) {
    return (length * sins(direction));
}
s16 LENCOS(s16 length, s16 direction) {
    return (length * coss(direction));
}

static void puppycam_analogue_stick(void) {
#ifdef TARGET_N64
    if (!gPuppyCam[0].options.analogue) {
        return;
    }
    // I make the X axis negative, so that the movement reflects the Cbuttons.
    gPuppyCam[gCurrentMario].stick2[0] = -gPlayer2Controller->rawStickX;
    gPuppyCam[gCurrentMario].stick2[1] =  gPlayer2Controller->rawStickY;

    if (ABS(gPuppyCam[gCurrentMario].stick2[0]) < DEADZONE) {
        gPuppyCam[gCurrentMario].stick2[0] = 0;
        gPuppyCam[gCurrentMario].stickN[0] = 0;
    }
    if (ABS(gPuppyCam[gCurrentMario].stick2[1]) < DEADZONE) {
        gPuppyCam[gCurrentMario].stick2[1] = 0;
        gPuppyCam[gCurrentMario].stickN[1] = 0;
    }
#endif
}

void puppycam_default_config(void) {
    gPuppyCam[0].options.invertX        = 1;
    gPuppyCam[0].options.invertY        = 1;
    gPuppyCam[0].options.sensitivityX   = 100;
    gPuppyCam[0].options.sensitivityY   = 100;
    gPuppyCam[0].options.turnAggression = 50;
    gPuppyCam[0].options.analogue       = 0;
    gPuppyCam[0].options.inputType      = 1;
}

// Initial setup. Ran at the beginning of the game and never again.
void puppycam_boot(void) {
    gPuppyCam[gCurrentMario].zoomPoints[0] = 600;
    gPuppyCam[gCurrentMario].zoomPoints[1] = 1000;
    gPuppyCam[gCurrentMario].zoomPoints[2] = 1500;
    gPuppyCam[gCurrentMario].povHeight     = 125;
    gPuppyCam[gCurrentMario].stick2[0]     = 0;
    gPuppyCam[gCurrentMario].stick2[1]     = 0;
    gPuppyCam[gCurrentMario].stickN[0]     = 0;
    gPuppyCam[gCurrentMario].stickN[1]     = 0;
    gPuppyMemoryPool        = mem_pool_init(MAX_PUPPYCAM_VOLUMES * sizeof(struct sPuppyVolume), MEMORY_POOL_LEFT);
    gPuppyVolumeCount       = 0;
    gPuppyCam[gCurrentMario].enabled       = 1;

    puppycam_get_save();
}

// Called when an instant warp is done.
void puppycam_warp(f32 displacementX, f32 displacementY, f32 displacementZ) {
    gPuppyCam[gCurrentMario].pos[0]                += displacementX;
    gPuppyCam[gCurrentMario].pos[1]                += displacementY;
    gPuppyCam[gCurrentMario].pos[2]                += displacementZ;
    gPuppyCam[gCurrentMario].targetFloorHeight     += displacementY;
    gPuppyCam[gCurrentMario].lastTargetFloorHeight += displacementY;
    gPuppyCam[gCurrentMario].floorY[0]             += displacementY;
    gPuppyCam[gCurrentMario].floorY[1]             += displacementY;
}

#if MULTILANG
static void newcam_set_language(void) {
    switch (gInGameLanguage - 1) {
    case LANGUAGE_ENGLISH:
        gPCOptionStringsPtr = &gPCOptionStringsEN;
        gPCFlagStringsPtr   = &gPCFlagStringsEN;
        gPCToggleStringsPtr = &gPCToggleStringsEN;
        break;
    case LANGUAGE_FRENCH:
        gPCOptionStringsPtr = &gPCOptionStringsFR;
        gPCFlagStringsPtr   = &gPCFlagStringsFR;
        gPCToggleStringsPtr = &gPCToggleStringsFR;
        break;
    case LANGUAGE_GERMAN:
        gPCOptionStringsPtr = &gPCOptionStringsDE;
        gPCFlagStringsPtr   = &gPCFlagStringsDE;
        gPCToggleStringsPtr = &gPCToggleStringsDE;
        break;
    }
}
#endif

///CUTSCENE

void puppycam_activate_cutscene(s32 (*scene)(), s32 lockinput) {
    gPuppyCam[gCurrentMario].cutscene   = 1;
    gPuppyCam[gCurrentMario].sceneTimer = 0;
    gPuppyCam[gCurrentMario].sceneFunc  = scene;
    gPuppyCam[gCurrentMario].sceneInput = lockinput;
}

// If you've read camera.c this will look familiar.
// It takes the next 4 spline points and extrapolates a curvature based positioning of the camera vector that's passed through.
// It's a standard B spline
static void puppycam_evaluate_spline(f32 progress, Vec3s cameraPos, Vec3f spline1, Vec3f spline2, Vec3f spline3, Vec3f spline4) {
    f32 tempP[4];

    if (progress > 1.0f) {
        progress = 1.0f;
    }

    tempP[0] = (1.0f - progress) * (1.0f - progress) * (1.0f - progress) / 6.0f;
    tempP[1] = progress * progress * progress / 2.0f - progress * progress + 0.6666667f;
    tempP[2] = -progress * progress * progress / 2.0f + progress * progress / 2.0f + progress / 2.0f + 0.16666667f;
    tempP[3] = progress * progress * progress / 6.0f;

    cameraPos[0] = tempP[0] * spline1[0] + tempP[1] * spline2[0] + tempP[2] * spline3[0] + tempP[3] * spline4[0];
    cameraPos[1] = tempP[0] * spline1[1] + tempP[1] * spline2[1] + tempP[2] * spline3[1] + tempP[3] * spline4[1];
    cameraPos[2] = tempP[0] * spline1[2] + tempP[1] * spline2[2] + tempP[2] * spline3[2] + tempP[3] * spline4[2];
}

s32 puppycam_move_spline(struct sPuppySpline splinePos[], struct sPuppySpline splineFocus[], s32 mode, s32 index) {
    Vec3f tempPoints[4];
    f32 tempProgress[2] = {0.0f, 0.0f};
    f32 progChange = 0.0f;
    s32 i;
    Vec3f prevPos;

    if (gPuppyCam[gCurrentMario].splineIndex == 65000) {
        gPuppyCam[gCurrentMario].splineIndex = index;
    }
    if (splinePos[gPuppyCam[gCurrentMario].splineIndex].index == -1 || splinePos[gPuppyCam[gCurrentMario].splineIndex + 1].index == -1 || splinePos[gPuppyCam[gCurrentMario].splineIndex + 2].index == -1) {
        return TRUE;
    }
    if (mode == PUPPYSPLINE_FOLLOW) {
        if (splineFocus[gPuppyCam[gCurrentMario].splineIndex].index == -1 || splineFocus[gPuppyCam[gCurrentMario].splineIndex + 1].index == -1 || splineFocus[gPuppyCam[gCurrentMario].splineIndex + 2].index == -1) {
            return TRUE;
        }
    }
    vec3f_set(prevPos, gPuppyCam[gCurrentMario].pos[0], gPuppyCam[gCurrentMario].pos[1], gPuppyCam[gCurrentMario].pos[2]);

    for (i = 0; i < 4; i++) {
        vec3f_set(tempPoints[i], splinePos[gPuppyCam[gCurrentMario].splineIndex + i].pos[0], splinePos[gPuppyCam[gCurrentMario].splineIndex + i].pos[1], splinePos[gPuppyCam[gCurrentMario].splineIndex + i].pos[2]);
    }
    puppycam_evaluate_spline(gPuppyCam[gCurrentMario].splineProgress, gPuppyCam[gCurrentMario].pos, tempPoints[0], tempPoints[1], tempPoints[2], tempPoints[3]);
    if (mode == PUPPYSPLINE_FOLLOW) {
        for (i = 0; i < 4; i++) {
            vec3f_set(tempPoints[i], splineFocus[gPuppyCam[gCurrentMario].splineIndex + i].pos[0], splineFocus[gPuppyCam[gCurrentMario].splineIndex + i].pos[1], splineFocus[gPuppyCam[gCurrentMario].splineIndex + i].pos[2]);
        }
        puppycam_evaluate_spline(gPuppyCam[gCurrentMario].splineProgress, gPuppyCam[gCurrentMario].focus, tempPoints[0], tempPoints[1], tempPoints[2], tempPoints[3]);
    }

    if (splinePos[gPuppyCam[gCurrentMario].splineIndex+1].speed != 0) {
        tempProgress[0] = 1.0f / splinePos[gPuppyCam[gCurrentMario].splineIndex+1].speed;
    }
    if (splinePos[gPuppyCam[gCurrentMario].splineIndex+2].speed != 0) {
        tempProgress[1] = 1.0f / splinePos[gPuppyCam[gCurrentMario].splineIndex+2].speed;
    }
    progChange = (tempProgress[1] - tempProgress[0]) * gPuppyCam[gCurrentMario].splineProgress + tempProgress[0];

    gPuppyCam[gCurrentMario].splineProgress += progChange;

    if (gPuppyCam[gCurrentMario].splineProgress >= 1.0f) {
        gPuppyCam[gCurrentMario].splineIndex++;
        if (splinePos[gPuppyCam[gCurrentMario].splineIndex + 3].index == -1) {
            gPuppyCam[gCurrentMario].splineIndex = 0;
            gPuppyCam[gCurrentMario].splineProgress = 0;
            return TRUE;
        }
        gPuppyCam[gCurrentMario].splineProgress -=1;
    }

    return FALSE;
}

static void puppycam_process_cutscene(void) {
    if (gPuppyCam[gCurrentMario].cutscene) {
        if ((gPuppyCam[gCurrentMario].sceneFunc)() == 1) {
            gPuppyCam[gCurrentMario].cutscene = 0;
            gPuppyCam[gCurrentMario].sceneInput = 0;
            gPuppyCam[gCurrentMario].flags = gPuppyCam[gCurrentMario].intendedFlags;
        }
        gPuppyCam[gCurrentMario].sceneTimer++;
    }
}

///MENU

#define BLANK 0, 0, 0, ENVIRONMENT, 0, 0, 0, ENVIRONMENT

static void puppycam_display_box(s32 x1, s32 y1, s32 x2, s32 y2, u8 r, u8 g, u8 b, u8 a) {
    gDPSetCombineMode(gDisplayListHead++, BLANK, BLANK);
    gDPSetCycleType(  gDisplayListHead++, G_CYC_1CYCLE);
    if (a !=255) {
        gDPSetRenderMode(gDisplayListHead++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    } else {
        gDPSetRenderMode(gDisplayListHead++, G_RM_OPA_SURF, G_RM_OPA_SURF);
    }
    gDPSetEnvColor(   gDisplayListHead++, r, g, b, a);
    gDPFillRectangle( gDisplayListHead++, x1, y1, x2, y2);
    gDPPipeSync(      gDisplayListHead++);
    gDPSetEnvColor(   gDisplayListHead++, 255, 255, 255, 255);
    gDPSetCycleType(  gDisplayListHead++, G_CYC_1CYCLE);
    gSPDisplayList(   gDisplayListHead++,dl_hud_img_end);
}

//I actually took the time to redo this, properly. Lmao. Please don't bully me over this anymore :(
void puppycam_change_setting(s8 toggle) {
    if (gPlayer1Controller->buttonDown & A_BUTTON) toggle *=  5;
    if (gPlayer1Controller->buttonDown & B_BUTTON) toggle *= 10;

    if (gPCOptions[gPCOptionSelected].gPCOptionMin == FALSE && gPCOptions[gPCOptionSelected].gPCOptionMax == TRUE) {
        *gPCOptions[gPCOptionSelected].gPCOptionVar ^= 1;
    } else {
        *gPCOptions[gPCOptionSelected].gPCOptionVar += toggle;
    }
    // Forgive me father, for I have sinned. I guess if you wanted a selling point for a 21:9 monitor though, "I can view this line in puppycam's code without scrolling!" can be added to it.
    *gPCOptions[gPCOptionSelected].gPCOptionVar = CLAMP(*gPCOptions[gPCOptionSelected].gPCOptionVar, gPCOptions[gPCOptionSelected].gPCOptionMin, gPCOptions[gPCOptionSelected].gPCOptionMax);

#if defined(VERSION_EU)
    newcam_set_language();
#endif
}

void puppycam_print_text(s32 x, s32 y, unsigned char *str, s32 col) {
    s32 textX = get_str_x_pos_from_center(x, str, 10.0f);
    gDPSetEnvColor(gDisplayListHead++, 0, 0, 0, 255);
    print_generic_string(textX + 1, y - 1,str);
    if (col != 0) {
        gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    } else {
        gDPSetEnvColor(gDisplayListHead++, 255,  32,  32, 255);
    }
    print_generic_string(textX,y,str);
}

// Options menu
void puppycam_display_options() {
    s32 i = 0;
    unsigned char newstring[32];
    s32 scroll;
    s32 scrollpos;
    u32 var; // should be s16, but gives a build warning otherwise?
    // s32 vr;
    s32 minvar, maxvar;
    f32 newcam_sinpos;

    puppycam_display_box( 47,  83, 281,  84, 0x00, 0x00, 0x00, 0xFF);
    puppycam_display_box( 47, 218, 281, 219, 0x00, 0x00, 0x00, 0xFF);
    puppycam_display_box( 47,  83,  48, 219, 0x00, 0x00, 0x00, 0xFF);
    puppycam_display_box(280,  83, 281, 219, 0x00, 0x00, 0x00, 0xFF);
    puppycam_display_box(271,  83, 272, 219, 0x00, 0x00, 0x00, 0xFF);

    puppycam_display_box(48,84,272,218,0x0,0x0,0x0, 0x50);
    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_begin);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    print_hud_lut_string(HUD_LUT_GLOBAL, 112, 40, (*gPCToggleStringsPtr)[2]);
    gSPDisplayList(gDisplayListHead++, dl_rgba16_text_end);

    if (gPCOptionCap > 4) {
        puppycam_display_box(272, 84, 280, 218, 0x80, 0x80, 0x80, 0xFF);
        scrollpos = (62) * ((f32)gPCOptionScroll / (gPCOptionCap - 4));
        puppycam_display_box(272, 84 + scrollpos, 280, 156 + scrollpos, 0xFF, 0xFF, 0xFF, 0xFF);
    }

    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, 80, SCREEN_WIDTH, SCREEN_HEIGHT);
    for (i = 0; i < gPCOptionCap; i++) {
        scroll = 140 - (32 * i) + (gPCOptionScroll * 32);
        if (scroll <= 140 && scroll > 32) {
            puppycam_print_text(160,scroll,(*gPCOptionStringsPtr)[gPCOptions[i].gPCOptionName],gPCOptionSelected-i);
            if (gPCOptions[i].gPCOptionStart != 255) {
                var = *gPCOptions[i].gPCOptionVar+gPCOptions[i].gPCOptionStart;
                if (var < sizeof(gPCFlagStringsEN)) { // Failsafe for if it somehow indexes an out of bounds array.
                    puppycam_print_text(160, scroll - 12, (*gPCFlagStringsPtr)[var], gPCOptionSelected - i);
                }
            } else {
                int_to_str(*gPCOptions[i].gPCOptionVar,newstring);
                puppycam_print_text(160,scroll-12,newstring,gPCOptionSelected-i);
                puppycam_display_box(96, 111 + (32 * i) - (gPCOptionScroll * 32), 224, 117 + (32 * i) - (gPCOptionScroll * 32), 0x80, 0x80, 0x80, 0xFF);
                maxvar = gPCOptions[i].gPCOptionMax - gPCOptions[i].gPCOptionMin;
                minvar = *gPCOptions[i].gPCOptionVar - gPCOptions[i].gPCOptionMin;
                puppycam_display_box(96, 111 + (32 * i) - (gPCOptionScroll * 32), 96 + (((f32)minvar / maxvar) * 128), 117 + (32 * i) - (gPCOptionScroll * 32), 0xFF, 0xFF, 0xFF, 0xFF);
                puppycam_display_box(94 + (((f32)minvar / maxvar) * 128), 109 + (32 * i) - (gPCOptionScroll * 32), 98 + (((f32)minvar / maxvar) * 128), 119 + (32 * i) - (gPCOptionScroll * 32), 0xFF, 0x00, 0x00, 0xFF);
                gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
            }
        }
    }
    newcam_sinpos = sins(gGlobalTimer * 5000) * 4;
    gDPSetScissor(gDisplayListHead++, G_SC_NON_INTERLACE, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    gDPSetEnvColor(gDisplayListHead++, 255, 255, 255, 255);
    print_generic_string( 80 - newcam_sinpos, 132 - (32 * (gPCOptionSelected - gPCOptionScroll)),  (*gPCToggleStringsPtr)[3]);
    print_generic_string(232 + newcam_sinpos, 132 - (32 * (gPCOptionSelected - gPCOptionScroll)),  (*gPCToggleStringsPtr)[4]);
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}

//This has been separated for interesting reasons. Don't question it.
void puppycam_render_option_text(void) {
    gSPDisplayList(gDisplayListHead++, dl_ia_text_begin);
    puppycam_print_text(278,212,(*gPCToggleStringsPtr)[gPCOptionOpen],1);
    gSPDisplayList(gDisplayListHead++, dl_ia_text_end);
}

extern struct SaveBuffer gSaveBuffer;

void puppycam_check_pause_buttons(void) {
    if (gPlayer1Controller->buttonPressed & R_TRIG) {
        play_sound(SOUND_MENU_CHANGE_SELECT, gGlobalSoundSource);
        if (gPCOptionOpen == 0) {
            gPCOptionOpen = 1;
#if MULTILANG
            newcam_set_language();
            eu_set_language(gInGameLanguage-1);
#endif
        } else {
            gPCOptionOpen = 0;
#if MULTILANG
            load_language_text();
#endif
            puppycam_set_save();
        }
    }

    if (gPCOptionOpen) {
        if (ABS(gPlayer1Controller->rawStickY) > 60 || gPlayer1Controller->buttonDown & U_JPAD || gPlayer1Controller->buttonDown & D_JPAD) {
            gPCOptionTimer--;
            if (gPCOptionTimer <= 0) {
                if (gPCOptionIndex == 0) {
                    gPCOptionIndex++;
                    gPCOptionTimer += 10;
                } else {
                    gPCOptionTimer +=  5;
                }
                play_sound(SOUND_MENU_CHANGE_SELECT, gGlobalSoundSource);
                if (gPlayer1Controller->rawStickY >= 60 || gPlayer1Controller->buttonDown & U_JPAD) {
                    gPCOptionSelected--;
                    if (gPCOptionSelected < 0) {
                        gPCOptionSelected = gPCOptionCap - 1;
                    }
                } else if (gPlayer1Controller->rawStickY <= -60 || gPlayer1Controller->buttonDown & D_JPAD) {
                    gPCOptionSelected++;
                    if (gPCOptionSelected >= gPCOptionCap) {
                        gPCOptionSelected = 0;
                    }
                }
            }
        } else if (ABS(gPlayer1Controller->rawStickX) > 60 || gPlayer1Controller->buttonDown & L_JPAD || gPlayer1Controller->buttonDown & R_JPAD) {
            gPCOptionTimer--;
            if (gPCOptionTimer <= 0) {
                switch (gPCOptionIndex) {
                    case 0: gPCOptionIndex++; gPCOptionTimer += 10; break;
                    default: gPCOptionTimer += 5; break;
                }
                play_sound(SOUND_MENU_CHANGE_SELECT, gGlobalSoundSource);
                if (gPlayer1Controller->rawStickX >= 60 || gPlayer1Controller->buttonDown & R_JPAD) {
                    puppycam_change_setting(1);
                } else if (gPlayer1Controller->rawStickX <= -60 || gPlayer1Controller->buttonDown & L_JPAD) {
                    puppycam_change_setting(-1);
                }
            }
        } else {
            gPCOptionTimer = 0;
            gPCOptionIndex = 0;
        }
        while (gPCOptionScroll - gPCOptionSelected < -3 && gPCOptionSelected > gPCOptionScroll) gPCOptionScroll++;
        while (gPCOptionScroll + gPCOptionSelected >  0 && gPCOptionSelected < gPCOptionScroll) gPCOptionScroll--;
    }
}

///CORE

// Just a function that sets a bunch of camera values to 0. It's a function because it's got shared functionality.
void puppycam_reset_values(void) {
    gPuppyCam[gCurrentMario].swimPitch      = 0;
    gPuppyCam[gCurrentMario].edgePitch      = 0;
    gPuppyCam[gCurrentMario].moveZoom       = 0;
    gPuppyCam[gCurrentMario].floorY[0]      = 0;
    gPuppyCam[gCurrentMario].floorY[1]      = 0;
    gPuppyCam[gCurrentMario].terrainPitch   = 0;
    gPuppyCam[gCurrentMario].splineIndex    = 0;
    gPuppyCam[gCurrentMario].splineProgress = 0;
}

// Set up values. Runs on level load.
void puppycam_init(void) {
    if (gMarioState->marioObj) {
        gPuppyCam[gCurrentMario].targetObj = gMarioState->marioObj;
    }
    gPuppyCam[gCurrentMario].targetObj2 = NULL;

    gPuppyCam[gCurrentMario].intendedFlags = PUPPYCAM_BEHAVIOUR_DEFAULT;
#ifndef DISABLE_LEVEL_SPECIFIC_CHECKS
    if (gCurrLevelNum == LEVEL_PSS || (gCurrLevelNum == LEVEL_TTM && gCurrAreaIndex == 2) || (gCurrLevelNum == LEVEL_CCM && gCurrAreaIndex == 2)) {
        gPuppyCam[gCurrentMario].intendedFlags |= PUPPYCAM_BEHAVIOUR_SLIDE_CORRECTION;
    }
#endif
    gPuppyCam[gCurrentMario].flags                 = gPuppyCam[gCurrentMario].intendedFlags;
    gPuppyCam[gCurrentMario].zoom                  = gPuppyCam[gCurrentMario].zoomPoints[1];
    gPuppyCam[gCurrentMario].zoomSet               = 1;
    gPuppyCam[gCurrentMario].zoomTarget            = gPuppyCam[gCurrentMario].zoom;
    gPuppyCam[gCurrentMario].yaw                   = gMarioState->faceAngle[1] + 0x8000;
    gPuppyCam[gCurrentMario].yawTarget             = gPuppyCam[gCurrentMario].yaw;
    gPuppyCam[gCurrentMario].pitch                 = 0x3800;
    gPuppyCam[gCurrentMario].pitchTarget           = gPuppyCam[gCurrentMario].pitch;
    gPuppyCam[gCurrentMario].yawAcceleration       = 0;
    gPuppyCam[gCurrentMario].pitchAcceleration     = 0;
    gPuppyCam[gCurrentMario].shakeFrames           = 0;
    vec3_zero(gPuppyCam[gCurrentMario].shake);
    vec3_zero(gPuppyCam[gCurrentMario].pos);
    vec3_zero(gPuppyCam[gCurrentMario].focus);
    vec3_zero(gPuppyCam[gCurrentMario].pan); // gMarioState->pos[1];
    gPuppyCam[gCurrentMario].targetFloorHeight     = gPuppyCam[gCurrentMario].pan[1];
    gPuppyCam[gCurrentMario].lastTargetFloorHeight = gMarioState->pos[1];
    gPuppyCam[gCurrentMario].opacity               = 255;
    gPuppyCam[gCurrentMario].framesSinceC[0]       = 10; // This just exists to stop input type B being stupid.
    gPuppyCam[gCurrentMario].framesSinceC[1]       = 10; // This just exists to stop input type B being stupid.
    gPuppyCam[gCurrentMario].mode3Flags            = PUPPYCAM_MODE3_ZOOMED_MED;
    gPuppyCam[gCurrentMario].debugFlags            = PUPPYDEBUG_LOCK_CONTROLS;
    puppycam_reset_values();
}

void puppycam_input_pitch(void) {
    if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_PITCH_ROTATION) {
        // Handles vertical inputs.
        if (gPlayer1Controller->buttonDown & U_CBUTTONS || gPuppyCam[gCurrentMario].stick2[1] != 0) {
            gPuppyCam[gCurrentMario].pitchAcceleration -= 50 * (gPuppyCam[0].options.sensitivityY / 100.f);
        } else if (gPlayer1Controller->buttonDown & D_CBUTTONS || gPuppyCam[gCurrentMario].stick2[1] != 0) {
            gPuppyCam[gCurrentMario].pitchAcceleration += 50 * (gPuppyCam[0].options.sensitivityY / 100.f);
        } else {
            gPuppyCam[gCurrentMario].pitchAcceleration = 0;
        }
        gPuppyCam[gCurrentMario].pitchAcceleration = CLAMP(gPuppyCam[gCurrentMario].pitchAcceleration, -100, 100);

        // When Mario's moving, his pitch is clamped pretty aggressively, so this exists so you can shift your view up and down momentarily at an actually usable range, rather than the otherwise baby range.
        if (gMarioState->action & ACT_FLAG_MOVING && (gPuppyCam[gCurrentMario].pitch >= 0x3800 || gPuppyCam[gCurrentMario].pitch <= 0x2000)) {
            gPuppyCam[gCurrentMario].moveFlagAdd = 8;
        }
    }
}

void puppycam_input_zoom(void) {
    // Handles R button zooming.
    if (gPlayer1Controller->buttonPressed & R_TRIG && gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_ZOOM_CHANGE) {
        gPuppyCam[gCurrentMario].zoomSet++;

        if (gPuppyCam[gCurrentMario].zoomSet >= 3) {
            gPuppyCam[gCurrentMario].zoomSet = 0;
        }
        gPuppyCam[gCurrentMario].zoomTarget = gPuppyCam[gCurrentMario].zoomPoints[gPuppyCam[gCurrentMario].zoomSet];
        play_sound(SOUND_MENU_CLICK_CHANGE_VIEW,gGlobalSoundSource);
    }
}

void puppycam_input_centre(void) {
    if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_FREE) {
        return;
    }
    s32 inputDefault = L_TRIG;
    if (gPuppyCam[0].options.inputType == 2) {
        inputDefault = R_TRIG;
    }
    // Handles L button centering.
    if (gPlayer1Controller->buttonPressed & inputDefault && gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_YAW_ROTATION &&
    !(gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_INPUT_8DIR) && !(gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_INPUT_4DIR) && !(gPlayer1Controller->buttonDown & U_JPAD)) {
        gPuppyCam[gCurrentMario].yawTarget = gMarioState->faceAngle[1] + 0x8000;
        play_sound(SOUND_MENU_CLICK_CHANGE_VIEW,gGlobalSoundSource);
    }
}

// The default control scheme. Hold the button down to turn the camera, and double tap to turn quickly.
static void puppycam_input_hold_preset1(f32 ivX) {
    if (!gPuppyCam[0].options.analogue && gPlayer1Controller->buttonPressed & L_CBUTTONS && gPuppyCam[gCurrentMario].framesSinceC[0] <= 5) {
        gPuppyCam[gCurrentMario].yawTarget -= 0x4000 * ivX;
        play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gGlobalSoundSource);
    } else if (!gPuppyCam[0].options.analogue && gPlayer1Controller->buttonPressed & R_CBUTTONS && gPuppyCam[gCurrentMario].framesSinceC[1] <= 5) {
        gPuppyCam[gCurrentMario].yawTarget += 0x4000 * ivX;
        play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gGlobalSoundSource);
    }

    if ((gPlayer1Controller->buttonDown & L_CBUTTONS && !gPuppyCam[0].options.analogue) || gPuppyCam[gCurrentMario].stick2[0] != 0) {
        gPuppyCam[gCurrentMario].yawAcceleration -= 75 * (gPuppyCam[0].options.sensitivityX / 100.f);
        gPuppyCam[gCurrentMario].framesSinceC[0] = 0;
    } else if ((gPlayer1Controller->buttonDown & R_CBUTTONS && !gPuppyCam[0].options.analogue) || gPuppyCam[gCurrentMario].stick2[0] != 0) {
        gPuppyCam[gCurrentMario].yawAcceleration += 75*(gPuppyCam[0].options.sensitivityX / 100.f);
        gPuppyCam[gCurrentMario].framesSinceC[1] = 0;
    } else {
        gPuppyCam[gCurrentMario].yawAcceleration = 0;
    }
}

// An alternative control scheme, hold the button down to turn the camera, or press it once to turn it quickly.
static void puppycam_input_hold_preset2(f32 ivX) {
    // These set the initial button press.
    if (gPlayer1Controller->buttonPressed & L_CBUTTONS) gPuppyCam[gCurrentMario].framesSinceC[0] = 0;
    if (gPlayer1Controller->buttonPressed & R_CBUTTONS) gPuppyCam[gCurrentMario].framesSinceC[1] = 0;
    // These handle when you release the button
    if ((!(gPlayer1Controller->buttonDown & L_CBUTTONS)) && gPuppyCam[gCurrentMario].framesSinceC[0] <= 5) {
        gPuppyCam[gCurrentMario].yawTarget -= 0x3000 * ivX;
        play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gGlobalSoundSource);
        gPuppyCam[gCurrentMario].framesSinceC[0] = 6;
    }

    if ((!(gPlayer1Controller->buttonDown & R_CBUTTONS)) && gPuppyCam[gCurrentMario].framesSinceC[1] <= 5) {
        gPuppyCam[gCurrentMario].yawTarget += 0x3000 * ivX;
        play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gGlobalSoundSource);
        gPuppyCam[gCurrentMario].framesSinceC[1] = 6;
    }

    // Handles continuous movement as normal, as long as the button's held.
    if (gPlayer1Controller->buttonDown & L_CBUTTONS) {
        gPuppyCam[gCurrentMario].yawAcceleration -= 75 * (gPuppyCam[0].options.sensitivityX / 100.f);
    } else if (gPlayer1Controller->buttonDown & R_CBUTTONS) {
        gPuppyCam[gCurrentMario].yawAcceleration += 75 * (gPuppyCam[0].options.sensitivityX / 100.f);
    } else {
        gPuppyCam[gCurrentMario].yawAcceleration = 0;
    }
}

// Another alternative control scheme. This one aims to mimic the parallel camera scheme down to the last bit from the original game.
static void puppycam_input_hold_preset3(void) {
    f32 stickMag[2] = {gPlayer1Controller->rawStickX*0.65f, gPlayer1Controller->rawStickY*0.2f};
    // Just in case it happens to be nonzero.
    gPuppyCam[gCurrentMario].yawAcceleration = 0;

    //In theory this shouldn't be necessary, but it's nice to cover all bases.
    if (!(gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_YAW_ROTATION)) {
        return;
    }
    if (gPuppyCam[gCurrentMario].mode3Flags & PUPPYCAM_MODE3_ZOOMED_IN) {
        gPuppyCam[gCurrentMario].flags &= ~PUPPYCAM_BEHAVIOUR_COLLISION;

        // Handles continuous movement as normal, as long as the button's held.
        if (ABS(gPlayer1Controller->rawStickX) > DEADZONE) {
            gPuppyCam[gCurrentMario].yawAcceleration -= (gPuppyCam[0].options.sensitivityX / 100.f) * stickMag[0];
        } else {
            gPuppyCam[gCurrentMario].yawAcceleration = 0;
        }
        if (ABS(gPlayer1Controller->rawStickY) > DEADZONE) {
            gPuppyCam[gCurrentMario].pitchAcceleration -= (gPuppyCam[0].options.sensitivityY / 100.f) * stickMag[1];
        } else {
            gPuppyCam[gCurrentMario].pitchAcceleration = approach_f32_asymptotic(gPuppyCam[gCurrentMario].pitchAcceleration, 0, DECELERATION);
        }
    } else {
        if ((gPlayer1Controller->buttonPressed & L_TRIG) && (gPuppyCam[gCurrentMario].yawTarget % 0x2000)) {
            gPuppyCam[gCurrentMario].yawTarget += 0x2000 - gPuppyCam[gCurrentMario].yawTarget % 0x2000;
        }

        if (gPuppyCam[gCurrentMario].mode3Flags & PUPPYCAM_MODE3_ZOOMED_MED) gPuppyCam[gCurrentMario].pitchTarget = approach_s32(gPuppyCam[gCurrentMario].pitchTarget, 0x3800, 0x200, 0x200);
        if (gPuppyCam[gCurrentMario].mode3Flags & PUPPYCAM_MODE3_ZOOMED_OUT) gPuppyCam[gCurrentMario].pitchTarget = approach_s32(gPuppyCam[gCurrentMario].pitchTarget, 0x3000, 0x200, 0x200);

        if ((gPlayer1Controller->buttonPressed & L_CBUTTONS && !gPuppyCam[0].options.analogue) || (gPuppyCam[gCurrentMario].stick2[0] > DEADZONE && !gPuppyCam[gCurrentMario].stickN[0])) {
            gPuppyCam[gCurrentMario].stickN[0]  = 1;
            gPuppyCam[gCurrentMario].yawTarget -= 0x2000;
            play_sound(SOUND_MENU_CAMERA_TURN,gGlobalSoundSource);
        }
        if ((gPlayer1Controller->buttonPressed & R_CBUTTONS && !gPuppyCam[0].options.analogue) || (gPuppyCam[gCurrentMario].stick2[0] < -DEADZONE && !gPuppyCam[gCurrentMario].stickN[0])) {
            gPuppyCam[gCurrentMario].stickN[0]  = 1;
            gPuppyCam[gCurrentMario].yawTarget += 0x2000;
            play_sound(SOUND_MENU_CAMERA_TURN,gGlobalSoundSource);
        }
    }

    // Handles zooming in. Works just like vanilla.
    if ((gPlayer1Controller->buttonPressed & U_CBUTTONS && !gPuppyCam[0].options.analogue) || (gPuppyCam[gCurrentMario].stick2[1] > DEADZONE && !gPuppyCam[gCurrentMario].stickN[1])) {
        if ((gPuppyCam[gCurrentMario].mode3Flags & PUPPYCAM_MODE3_ZOOMED_MED) && !(gMarioState->action & ACT_FLAG_AIR) && !(gMarioState->action & ACT_FLAG_SWIMMING)) {
            gPuppyCam[gCurrentMario].stickN[1]   = 1;
            gPuppyCam[gCurrentMario].mode3Flags |= PUPPYCAM_MODE3_ZOOMED_IN;
            gPuppyCam[gCurrentMario].mode3Flags &= ~PUPPYCAM_MODE3_ZOOMED_MED;
            gPuppyCam[gCurrentMario].zoomTarget  = 200;
            gPuppyCam[gCurrentMario].mode3Flags |= PUPPYCAM_MODE3_ENTER_FIRST_PERSON;

            play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gGlobalSoundSource);
        } else if (gPuppyCam[gCurrentMario].mode3Flags & PUPPYCAM_MODE3_ZOOMED_OUT) {
            gPuppyCam[gCurrentMario].stickN[1]   = 1;
            gPuppyCam[gCurrentMario].mode3Flags |= PUPPYCAM_MODE3_ZOOMED_MED;
            gPuppyCam[gCurrentMario].mode3Flags &= ~PUPPYCAM_MODE3_ZOOMED_OUT;
            gPuppyCam[gCurrentMario].zoomTarget  = gPuppyCam[gCurrentMario].zoomPoints[1];

            play_sound(SOUND_MENU_CAMERA_ZOOM_IN, gGlobalSoundSource);
        }
    } else  if ((gPlayer1Controller->buttonPressed & D_CBUTTONS && !gPuppyCam[0].options.analogue) || (gPuppyCam[gCurrentMario].stick2[1] < -DEADZONE && !gPuppyCam[gCurrentMario].stickN[1])) { // Otherwise handle zooming out.
        if (gPuppyCam[gCurrentMario].mode3Flags & PUPPYCAM_MODE3_ZOOMED_MED) {
            gPuppyCam[gCurrentMario].stickN[1] = 1;
            gPuppyCam[gCurrentMario].mode3Flags |= PUPPYCAM_MODE3_ZOOMED_OUT;
            gPuppyCam[gCurrentMario].mode3Flags &= ~PUPPYCAM_MODE3_ZOOMED_MED;
            gPuppyCam[gCurrentMario].zoomTarget = gPuppyCam[gCurrentMario].zoomPoints[2];

            play_sound(SOUND_MENU_CAMERA_ZOOM_OUT, gGlobalSoundSource);
        }
    }
    if ((gPlayer1Controller->buttonPressed & D_CBUTTONS && !gPuppyCam[0].options.analogue) || (gPuppyCam[gCurrentMario].stick2[1] < -DEADZONE && !gPuppyCam[gCurrentMario].stickN[1]) ||
        gPlayer1Controller->buttonPressed & B_BUTTON || gPlayer1Controller->buttonPressed & A_BUTTON) {
        if (gPuppyCam[gCurrentMario].mode3Flags & PUPPYCAM_MODE3_ZOOMED_IN) {
            gPuppyCam[gCurrentMario].stickN[1]   = 1;
            gPuppyCam[gCurrentMario].mode3Flags |= PUPPYCAM_MODE3_ZOOMED_MED;
            gPuppyCam[gCurrentMario].mode3Flags &= ~PUPPYCAM_MODE3_ZOOMED_IN;
            gPuppyCam[gCurrentMario].zoomTarget  = gPuppyCam[gCurrentMario].zoomPoints[1];
            gPuppyCam[gCurrentMario].mode3Flags &= ~PUPPYCAM_MODE3_ENTER_FIRST_PERSON;

            play_sound(SOUND_MENU_CAMERA_ZOOM_OUT, gGlobalSoundSource);
        }
    }
}

// Handles C Button inputs for modes that have held inputs, rather than presses.
static void puppycam_input_hold(void) {
    f32 ivX = ((gPuppyCam[0].options.invertX * 2) - 1) * (gPuppyCam[0].options.sensitivityX / 100.f);
    f32 ivY = ((gPuppyCam[0].options.invertY * 2) - 1) * (gPuppyCam[0].options.sensitivityY / 100.f);
    s8 stickMag[2] = {100, 100};

    if (gPuppyCam[gCurrentMario].intendedFlags & PUPPYCAM_BEHAVIOUR_FREE) {
        gPuppyCam[gCurrentMario].flags = PUPPYCAM_BEHAVIOUR_FREE | PUPPYCAM_BEHAVIOUR_YAW_ROTATION | PUPPYCAM_BEHAVIOUR_PITCH_ROTATION;
    }
    // Analogue Camera stuff. If it fails to find an input, then it just sets stickmag to 100, which after calculations means the value goes unchanged.
    if (gPuppyCam[0].options.analogue && gPuppyCam[0].options.inputType != 2) {
        stickMag[0] = gPuppyCam[gCurrentMario].stick2[0] * 1.25f;
        stickMag[1] = gPuppyCam[gCurrentMario].stick2[1] * 1.25f;
    }

    //In theory this shouldn't be necessary, but it's nice to cover all bases.
    if (!(gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_YAW_ROTATION)) {
        return;
    }
    if ((!gPuppyCam[0].options.analogue || gPuppyCam[0].options.inputType == 2) && !(gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_FREE)) {
        switch (gPuppyCam[0].options.inputType) {
            default: puppycam_input_hold_preset1(ivX); puppycam_input_pitch(); puppycam_input_zoom(); puppycam_input_centre(); break;
            case 1:  puppycam_input_hold_preset2(ivX); puppycam_input_pitch(); puppycam_input_zoom(); puppycam_input_centre(); break;
            case 2:  puppycam_input_hold_preset3();                                                   puppycam_input_centre(); break;
        }
    } else {
        puppycam_input_hold_preset1(ivX);
        puppycam_input_pitch();
        puppycam_input_zoom();
        puppycam_input_centre();
    }

    gPuppyCam[gCurrentMario].framesSinceC[0]++;
    gPuppyCam[gCurrentMario].framesSinceC[1]++;

    gPuppyCam[gCurrentMario].yawAcceleration = CLAMP(gPuppyCam[gCurrentMario].yawAcceleration, -100, 100);

    gPuppyCam[gCurrentMario].yawTarget += (12 * gPuppyCam[gCurrentMario].yawAcceleration * ivX) * (stickMag[0] * 0.01f);
    gPuppyCam[gCurrentMario].pitchTarget += ((4 + gPuppyCam[gCurrentMario].moveFlagAdd) * gPuppyCam[gCurrentMario].pitchAcceleration * ivY) * (stickMag[1] * 0.01f);
}

// Handles C Button inputs for modes that have pressed inputs, rather than held.
static void puppycam_input_press(void) {
    f32 ivX = ((gPuppyCam[0].options.invertX * 2) - 1) * (gPuppyCam[0].options.sensitivityX / 100.f);
    f32 ivY = ((gPuppyCam[0].options.invertY * 2) - 1) * (gPuppyCam[0].options.sensitivityY / 100.f);
    s8 stickMag = 0;

    // Analogue Camera stuff. If it fails to find an input, then it just sets stickmag to 100, which after calculations means the value goes unchanged.
    if (gPuppyCam[0].options.analogue) {
        stickMag = gPuppyCam[gCurrentMario].stick2[0] * 1.25f;
    } else {
        stickMag = 100;
    }
    // Just in case it happens to be nonzero.
    gPuppyCam[gCurrentMario].yawAcceleration = 0;

    // In theory this shouldn't be necessary, but it's nice to cover all bases.
    if (!(gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_YAW_ROTATION)) {
        return;
    }
    if ((gPlayer1Controller->buttonPressed & L_CBUTTONS && !gPuppyCam[0].options.analogue) || (gPuppyCam[gCurrentMario].stickN[0] == 0 && gPuppyCam[gCurrentMario].stick2[0] < -DEADZONE)) {
        gPuppyCam[gCurrentMario].stickN[0] = 1;
        if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_INPUT_8DIR) {
            gPuppyCam[gCurrentMario].yawTarget -= 0x2000 * ivX;
        } else {
            gPuppyCam[gCurrentMario].yawTarget -= 0x4000 * ivX;
        }
        play_sound(SOUND_MENU_CAMERA_ZOOM_IN,gGlobalSoundSource);
    }

    if ((gPlayer1Controller->buttonPressed & R_CBUTTONS && !gPuppyCam[0].options.analogue) || (gPuppyCam[gCurrentMario].stickN[0] == 0 && gPuppyCam[gCurrentMario].stick2[0] > DEADZONE)) {
        gPuppyCam[gCurrentMario].stickN[0] = 1;
        if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_INPUT_8DIR) {
            gPuppyCam[gCurrentMario].yawTarget += 0x2000 * ivX;
        } else {
            gPuppyCam[gCurrentMario].yawTarget += 0x4000 * ivX;
        }
        play_sound(SOUND_MENU_CAMERA_ZOOM_IN,gGlobalSoundSource);
    }
    puppycam_input_pitch();
    puppycam_input_zoom();
    puppycam_input_centre();
    gPuppyCam[gCurrentMario].pitchTarget += ((4 + gPuppyCam[gCurrentMario].moveFlagAdd) * gPuppyCam[gCurrentMario].pitchAcceleration * ivY) * (stickMag * 0.01f);
}

void puppycam_debug_view(void) {
    if (gPuppyCam[gCurrentMario].debugFlags & PUPPYDEBUG_LOCK_CONTROLS) {
        if (ABS(gPlayer1Controller->rawStickX) > DEADZONE) {
            gPuppyCam[gCurrentMario].pos[0] += (gPlayer1Controller->rawStickX / 4) * -sins(gPuppyCam[gCurrentMario].yawTarget);
            gPuppyCam[gCurrentMario].pos[2] += (gPlayer1Controller->rawStickX / 4) *  coss(gPuppyCam[gCurrentMario].yawTarget);
        }
        if (ABS(gPlayer1Controller->rawStickY) > DEADZONE) {
            gPuppyCam[gCurrentMario].pos[0] += (gPlayer1Controller->rawStickY / 4) *  coss(gPuppyCam[gCurrentMario].yawTarget);
            gPuppyCam[gCurrentMario].pos[1] += (gPlayer1Controller->rawStickY / 4) *  sins(gPuppyCam[gCurrentMario].pitchTarget);
            gPuppyCam[gCurrentMario].pos[2] += (gPlayer1Controller->rawStickY / 4) *  sins(gPuppyCam[gCurrentMario].yawTarget);
        }
        if (gPlayer1Controller->buttonDown & Z_TRIG || gPlayer1Controller->buttonDown & L_TRIG) {
            gPuppyCam[gCurrentMario].pos[1] -= 20;
        }
        if (gPlayer1Controller->buttonDown & R_TRIG) {
            gPuppyCam[gCurrentMario].pos[1] += 20;
        }
        gPuppyCam[gCurrentMario].focus[0] = gPuppyCam[gCurrentMario].pos[0] + (100 * coss(gPuppyCam[gCurrentMario].yawTarget));
        gPuppyCam[gCurrentMario].focus[1] = gPuppyCam[gCurrentMario].pos[1] + (100 * sins(gPuppyCam[gCurrentMario].pitchTarget));
        gPuppyCam[gCurrentMario].focus[2] = gPuppyCam[gCurrentMario].pos[2] + (100 * sins(gPuppyCam[gCurrentMario].yawTarget));
    } else {
        if (gPuppyCam[gCurrentMario].debugFlags & PUPPYDEBUG_TRACK_MARIO) {
            vec3_copy(gPuppyCam[gCurrentMario].focus, &gPuppyCam[gCurrentMario].targetObj->oPosVec);
        }

        gPuppyCam[gCurrentMario].yawTarget   = atan2s(gPuppyCam[gCurrentMario].pos[2] - gPuppyCam[gCurrentMario].focus[2], gPuppyCam[gCurrentMario].pos[0] - gPuppyCam[gCurrentMario].focus[0]);
        gPuppyCam[gCurrentMario].pitchTarget = atan2s(gPuppyCam[gCurrentMario].pos[1] - gPuppyCam[gCurrentMario].focus[1], 100);
    }

    gPuppyCam[gCurrentMario].yaw   = gPuppyCam[gCurrentMario].yawTarget;
    gPuppyCam[gCurrentMario].pitch = gPuppyCam[gCurrentMario].pitchTarget;

    if (gPlayer1Controller->buttonPressed & A_BUTTON && gPuppyCam[gCurrentMario].debugFlags & PUPPYDEBUG_LOCK_CONTROLS) {
        vec3f_set(gMarioState->pos, gPuppyCam[gCurrentMario].pos[0], gPuppyCam[gCurrentMario].pos[1], gPuppyCam[gCurrentMario].pos[2]);
    }
    if (gPlayer1Controller->buttonPressed & B_BUTTON) {
        gPuppyCam[gCurrentMario].debugFlags ^= PUPPYDEBUG_LOCK_CONTROLS;
    }

    if (gPlayer1Controller->buttonPressed & R_TRIG && !(gPuppyCam[gCurrentMario].debugFlags & PUPPYDEBUG_LOCK_CONTROLS)) {
        gPuppyCam[gCurrentMario].debugFlags ^= PUPPYDEBUG_TRACK_MARIO;
    }
}

static void puppycam_view_panning(void) {
    s32 expectedPanX, expectedPanZ;
    s32 height = gPuppyCam[gCurrentMario].targetObj->oPosY;
    s32 panEx = (gPuppyCam[gCurrentMario].zoomTarget >= 1000) * 160; //Removes the basic panning when idling if the zoom level is at the closest.
    f32 slideSpeed = 1;

    f32 panMulti = CLAMP(gPuppyCam[gCurrentMario].zoom / (f32)gPuppyCam[gCurrentMario].zoomPoints[2], 0.f, 1.f);
    if (gPuppyCam[0].options.inputType == 2) {
        panMulti /= 2;
    }
    if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_PANSHIFT && gMarioState->action != ACT_HOLDING_BOWSER && gMarioState->action != ACT_SLEEPING && gMarioState->action != ACT_START_SLEEPING) {
        if (gMarioState->action & ACT_FLAG_BUTT_OR_STOMACH_SLIDE) {
            slideSpeed = 10;
        }
        expectedPanX = LENSIN(panEx + (200 * (gMarioState->forwardVel / 320.f)), gMarioState->faceAngle[1]) * panMulti;
        expectedPanZ = LENCOS(panEx + (200 * (gMarioState->forwardVel / 320.f)), gMarioState->faceAngle[1]) * panMulti;

        gPuppyCam[gCurrentMario].pan[0] = approach_f32_asymptotic(gPuppyCam[gCurrentMario].pan[0], expectedPanX, 0.02f*slideSpeed);
        gPuppyCam[gCurrentMario].pan[2] = approach_f32_asymptotic(gPuppyCam[gCurrentMario].pan[2], expectedPanZ, 0.02f*slideSpeed);
        if (gMarioState->vel[1] == 0.0f) {
            f32 panFloor = CLAMP(find_floor_height((s16)(gPuppyCam[gCurrentMario].targetObj->oPosX+expectedPanX), (s16)(gPuppyCam[gCurrentMario].targetObj->oPosY + 200),
            (s16)(gPuppyCam[gCurrentMario].targetObj->oPosZ+expectedPanZ)), gPuppyCam[gCurrentMario].targetObj->oPosY - 50,gPuppyCam[gCurrentMario].targetObj->oPosY + 50);
            // If the floor is lower than 150 units below Mario, then ignore the Y value and tilt the camera instead.
            if (panFloor <= gPuppyCam[gCurrentMario].targetObj->oPosY - 150) {
                panFloor = gPuppyCam[gCurrentMario].targetObj->oPosY;
                gPuppyCam[gCurrentMario].edgePitch = approach_s32(gPuppyCam[gCurrentMario].edgePitch, -0x2000, 0x80, 0x80);
            } else {
                gPuppyCam[gCurrentMario].edgePitch = approach_s32(gPuppyCam[gCurrentMario].edgePitch, 0, 0x100, 0x100);
            }
            gPuppyCam[gCurrentMario].pan[1] = approach_f32_asymptotic(gPuppyCam[gCurrentMario].pan[1], panFloor - height, 0.025f);
        } else {
            gPuppyCam[gCurrentMario].pan[1] = approach_f32_asymptotic(gPuppyCam[gCurrentMario].pan[1], 0, 0.05f);
        }
    } else {
        vec3_zero(gPuppyCam[gCurrentMario].pan);
    }
}

void puppycam_terrain_angle(void) {
    f32 adjustSpeed;
    s32 floor2 = find_floor_height(gPuppyCam[gCurrentMario].pos[0], gPuppyCam[gCurrentMario].pos[1]+100, gPuppyCam[gCurrentMario].pos[2]);
    s32 ceil = 20000;//find_ceil(gPuppyCam[gCurrentMario].pos[0], gPuppyCam[gCurrentMario].pos[1]+100, gPuppyCam[gCurrentMario].pos[2]);
    s32 farFromSurface;
    s16 floorPitch;
    s32 gotTheOkay = FALSE;

    if (gMarioState->action & ACT_FLAG_SWIMMING || !(gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_HEIGHT_HELPER)) {
        gPuppyCam[gCurrentMario].intendedTerrainPitch = 0;
        adjustSpeed = 0.25f;
        farFromSurface = TRUE;
    } else {
        adjustSpeed = CLAMP(MAX(gMarioState->forwardVel/400.0f, gPuppyCam[gCurrentMario].yawAcceleration/100.0f), 0.05f, 1.0f);

        f32 x = gPuppyCam[gCurrentMario].targetObj->oPosX - (10 * sins(gPuppyCam[gCurrentMario].yaw));
        f32 z = gPuppyCam[gCurrentMario].targetObj->oPosZ - (10 * coss(gPuppyCam[gCurrentMario].yaw));

        f32 floorHeight = find_floor_height(x, gPuppyCam[gCurrentMario].targetObj->oPosY+100, z);
        f32 diff = gMarioState->floorHeight - floorHeight;

        if (ABS(diff) > 50.f) {
            gPuppyCam[gCurrentMario].intendedTerrainPitch = 0;
        } else {
            floorPitch = -atan2s(30.0f, diff);
            gPuppyCam[gCurrentMario].intendedTerrainPitch = approach_f32_asymptotic(gPuppyCam[gCurrentMario].intendedTerrainPitch, floorPitch, adjustSpeed);
            gotTheOkay = TRUE;
        }

        // Ensures that the camera is below and above floors and ceilings. It ignores this rule for each if the camera's headed upwards anyway.
        farFromSurface = ((gPuppyCam[gCurrentMario].pos[1] > floor2 + 50 || gPuppyCam[gCurrentMario].intendedTerrainPitch < gPuppyCam[gCurrentMario].terrainPitch) && (gPuppyCam[gCurrentMario].pos[1] < ceil - 50 || gPuppyCam[gCurrentMario].intendedTerrainPitch > gPuppyCam[gCurrentMario].terrainPitch));

        // If the camera is too close to a vertical obstruction, it'll make the intended pitch much further away, making it swivel faster.
        if (!farFromSurface && gotTheOkay) {
            gPuppyCam[gCurrentMario].intendedTerrainPitch = approach_f32_asymptotic(gPuppyCam[gCurrentMario].intendedTerrainPitch, floorPitch, adjustSpeed*3);
        }
    }

    if (farFromSurface) {
        gPuppyCam[gCurrentMario].terrainPitch = approach_f32_asymptotic(gPuppyCam[gCurrentMario].terrainPitch, gPuppyCam[gCurrentMario].intendedTerrainPitch, adjustSpeed);
    }
}

const struct sPuppyAngles puppyAnglesNull = {
    {PUPPY_NULL, PUPPY_NULL, PUPPY_NULL},
    {PUPPY_NULL, PUPPY_NULL, PUPPY_NULL},
    PUPPY_NULL,
    PUPPY_NULL,
    PUPPY_NULL,
};

// Checks the bounding box of a puppycam volume. If it's inside, then set the pointer to the current index.
static s32 puppycam_check_volume_bounds(struct sPuppyVolume *volume, s32 index) {
    s32 rel[3];
    s32 pos[2];

    if (sPuppyVolumeStack[index]->room != gMarioCurrentRoom && sPuppyVolumeStack[index]->room != -1) {
        return FALSE;
    }
    if (sPuppyVolumeStack[index]->shape == PUPPYVOLUME_SHAPE_BOX) {
        // Fetch the relative position. to the triggeree.
        vec3_diff(rel, sPuppyVolumeStack[index]->pos, &gPuppyCam[gCurrentMario].targetObj->oPosVec);
        // Use the dark, forbidden arts of trig to rotate the volume.
        pos[0] = rel[2] * sins(sPuppyVolumeStack[index]->rot) + rel[0] * coss(sPuppyVolumeStack[index]->rot);
        pos[1] = rel[2] * coss(sPuppyVolumeStack[index]->rot) - rel[0] * sins(sPuppyVolumeStack[index]->rot);
#ifdef VISUAL_DEBUG
        Vec3f debugPos[2];
        vec3f_set(debugPos[0], sPuppyVolumeStack[index]->pos[0],    sPuppyVolumeStack[index]->pos[1],    sPuppyVolumeStack[index]->pos[2]);
        vec3f_set(debugPos[1], sPuppyVolumeStack[index]->radius[0], sPuppyVolumeStack[index]->radius[1], sPuppyVolumeStack[index]->radius[2]);
        debug_box_color(0x0000FF00);
        debug_box_rot(debugPos[0], debugPos[1], sPuppyVolumeStack[index]->rot, DEBUG_SHAPE_BOX | DEBUG_UCODE_DEFAULT);
#endif
        // Now compare values.
        if (-sPuppyVolumeStack[index]->radius[0] < pos[0] && pos[0] < sPuppyVolumeStack[index]->radius[0] &&
            -sPuppyVolumeStack[index]->radius[1] < rel[1] && rel[1] < sPuppyVolumeStack[index]->radius[1] &&
            -sPuppyVolumeStack[index]->radius[2] < pos[1] && pos[1] < sPuppyVolumeStack[index]->radius[2]) {
            *volume = *sPuppyVolumeStack[index];
            return TRUE;
        }
    } else if (sPuppyVolumeStack[index]->shape == PUPPYVOLUME_SHAPE_CYLINDER) {
        // s16 dir;
        vec3_diff(rel, sPuppyVolumeStack[index]->pos, &gPuppyCam[gCurrentMario].targetObj->oPosVec);
        f32 dist = (sqr(rel[0]) + sqr(rel[2]));
#ifdef VISUAL_DEBUG
        Vec3f debugPos[2];
        vec3f_set(debugPos[0], sPuppyVolumeStack[index]->pos[0],    sPuppyVolumeStack[index]->pos[1],    sPuppyVolumeStack[index]->pos[2]);
        vec3f_set(debugPos[1], sPuppyVolumeStack[index]->radius[0], sPuppyVolumeStack[index]->radius[1], sPuppyVolumeStack[index]->radius[2]);
        debug_box_color(0x0000FF00);
        debug_box_rot(debugPos[0], debugPos[1], sPuppyVolumeStack[index]->rot, DEBUG_SHAPE_CYLINDER | DEBUG_UCODE_DEFAULT);
#endif
        f32 distCheck = (dist < sqr(sPuppyVolumeStack[index]->radius[0]));

        if (-sPuppyVolumeStack[index]->radius[1] < rel[1] && rel[1] < sPuppyVolumeStack[index]->radius[1] && distCheck) {
            *volume = *sPuppyVolumeStack[index];
            return TRUE;
        }

    }

    return FALSE;
}

// Handles wall adjustment when wall kicking.
void puppycam_wall_angle(void) {
    struct Surface *wall;
    struct WallCollisionData cData;

    if (!(gMarioState->action & ACT_WALL_KICK_AIR) || ((gMarioState->action & ACT_FLAG_AIR) && ABS(gMarioState->forwardVel) < 16.0f) || !(gMarioState->action & ACT_FLAG_AIR)) {
        return;
    }
    cData.x = gPuppyCam[gCurrentMario].targetObj->oPosX;
    cData.y = gPuppyCam[gCurrentMario].targetObj->oPosY;
    cData.z = gPuppyCam[gCurrentMario].targetObj->oPosZ;
    cData.radius = 150.0f;
    cData.offsetY = 0;

    if (find_wall_collisions(&cData)) {
        wall = cData.walls[cData.numWalls - 1];
    } else {
        return;
    }
    s16 wallYaw = atan2s(wall->normal.z, wall->normal.x) + 0x4000;

    wallYaw -= gPuppyCam[gCurrentMario].yawTarget;
    if (wallYaw % 0x4000) {
        wallYaw += 0x4000 - wallYaw % 0x4000;
    }
    gPuppyCam[gCurrentMario].yawTarget = approach_s32(gPuppyCam[gCurrentMario].yawTarget, wallYaw, 0x200, 0x200);
}

void puppycam_projection_behaviours(void) {
    f32 turnRate = 1;
    f32 turnMag = ABS(gPlayer1Controller->rawStickX / 80.0f);

    // This will only be executed if Mario's the target. If it's not, it'll reset the
    if (gPuppyCam[gCurrentMario].targetObj == gMarioState->marioObj) {
        if ((gPuppyCam[0].options.turnAggression > 0 && gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_TURN_HELPER && !(gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_INPUT_8DIR) &&
        gMarioState->vel[1] == 0.0f && !(gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_INPUT_4DIR) && gPuppyCam[0].options.inputType != 2) || gMarioState->action == ACT_SHOT_FROM_CANNON || gMarioState->action == ACT_FLYING) { // Holy hell this is getting spicy.
            // With turn aggression enabled, or if Mario's sliding, adjust the camera view behind mario.
            if (gPuppyCam[0].options.turnAggression > 0 || gMarioState->action & ACT_FLAG_BUTT_OR_STOMACH_SLIDE || gMarioState->action == ACT_SHOT_FROM_CANNON || gMarioState->action == ACT_FLYING) {
                if (gMarioState->action & ACT_FLAG_BUTT_OR_STOMACH_SLIDE) {
                    turnRate = 4; // If he's sliding, do it 4x as fast.
                }
                if (gMarioState->action == ACT_SHOT_FROM_CANNON || gMarioState->action == ACT_FLYING)
                    turnMag = 1;
                // The deal here, is if Mario's moving, or he's sliding and the camera's within 90 degrees behind him, it'll auto focus behind him, with an intensity based on the camera's centre speed.
                // It also scales with forward velocity, so it's a gradual effect as he speeds up.
                if ((ABS(gPlayer1Controller->rawStickX) > 20 && !(gMarioState->action & ACT_FLAG_BUTT_OR_STOMACH_SLIDE)) || (gMarioState->action == ACT_SHOT_FROM_CANNON) ||
                    (gMarioState->action & ACT_FLAG_BUTT_OR_STOMACH_SLIDE && (s16)ABS(((gPuppyCam[gCurrentMario].yaw + 0x8000) % 0xFFFF - 0x8000) - ((gMarioState->faceAngle[1]) % 0xFFFF - 0x8000)) < 0x3000 ))
                gPuppyCam[gCurrentMario].yawTarget = approach_angle(gPuppyCam[gCurrentMario].yawTarget, (gMarioState->faceAngle[1] + 0x8000), ((gPuppyCam[0].options.turnAggression * 10) * ABS(gMarioState->forwardVel / 32) * turnMag * turnRate));
            }
        } else { //If none of the above is true, it'll attempt to do this instead.
            // If the camera's in these modes, snap the yaw to prevent desync.
            if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_INPUT_8DIR) {
                if (gPuppyCam[gCurrentMario].yawTarget % 0x2000) {
                    gPuppyCam[gCurrentMario].yawTarget += 0x2000 - gPuppyCam[gCurrentMario].yawTarget % 0x2000;
                }
            }
            if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_INPUT_4DIR) {
                if (gPuppyCam[gCurrentMario].yawTarget % 0x4000) {
                    gPuppyCam[gCurrentMario].yawTarget += 0x4000 - gPuppyCam[gCurrentMario].yawTarget % 0x4000;
                }
            }
        }

        //This is the base floor height when stood on the ground. It's used to set a baseline for where the camera sits while Mario remains a height from this point, so it keeps a consistent motion.
        gPuppyCam[gCurrentMario].targetFloorHeight = CLAMP(find_floor_height(gPuppyCam[gCurrentMario].targetObj->oPosX, gPuppyCam[gCurrentMario].targetObj->oPosY, gPuppyCam[gCurrentMario].targetObj->oPosZ), gPuppyCam[gCurrentMario].targetObj->oPosY-350, gPuppyCam[gCurrentMario].targetObj->oPosY+300);
        gPuppyCam[gCurrentMario].lastTargetFloorHeight = approach_f32_asymptotic(gPuppyCam[gCurrentMario].lastTargetFloorHeight , gPuppyCam[gCurrentMario].targetFloorHeight
                                                                , CLAMP((absf(gMarioState->vel[1]) - 17.f) / 200.f, 0, 0.1f)
                                                                + CLAMP((absf(gPuppyCam[gCurrentMario].targetFloorHeight - gPuppyCam[gCurrentMario].lastTargetFloorHeight) - 30.f) / 300.f, 0, 0.1f));

        if (gMarioState->action == ACT_SLEEPING || gMarioState->action == ACT_START_SLEEPING) {
            gPuppyCam[gCurrentMario].zoom = approach_f32_asymptotic(gPuppyCam[gCurrentMario].zoom,gPuppyCam[gCurrentMario].zoomPoints[0],0.01f);
        } else if ((gMarioState->action & ACT_FLAG_SWIMMING_OR_FLYING && gMarioState->waterLevel-100 - gMarioState->pos[1] > 5) || gMarioState->action == ACT_FLYING) {
            // When moving underwater or flying, the camera will zoom in on Mayro.
            gPuppyCam[gCurrentMario].zoom = approach_f32_asymptotic(gPuppyCam[gCurrentMario].zoom, MAX(gPuppyCam[gCurrentMario].zoomTarget/1.5f, gPuppyCam[gCurrentMario].zoomPoints[0]), 0.2f);
        } else {
            gPuppyCam[gCurrentMario].zoom = approach_f32_asymptotic(gPuppyCam[gCurrentMario].zoom,gPuppyCam[gCurrentMario].zoomTarget, 0.2f);
        }
        // Attempts at automatic adjustment that only apply when moving or jumping.
        if (gMarioState->action & ACT_FLAG_MOVING || gMarioState->action & ACT_FLAG_AIR || (gMarioState->action & ACT_FLAG_SWIMMING && !gMarioState->waterLevel-100 - gMarioState->pos[1] > 5 && gMarioState->forwardVel != 0.0f)) {
            // Clamp the height when moving. You can still look up and down to a reasonable degree but it readjusts itself the second you let go.
            if (gPuppyCam[gCurrentMario].pitchTarget > 0x3800) gPuppyCam[gCurrentMario].pitchTarget = approach_f32_asymptotic(gPuppyCam[gCurrentMario].pitchTarget, 0x3800, 0.2f);
            if (gPuppyCam[gCurrentMario].pitchTarget < 0x2000) gPuppyCam[gCurrentMario].pitchTarget = approach_f32_asymptotic(gPuppyCam[gCurrentMario].pitchTarget, 0x2000, 0.2f);
        }

        // Applies a light outward zoom to the camera when moving. Sets it back to 0 when not moving.
        if (gMarioState->forwardVel > 0) {
            gPuppyCam[gCurrentMario].moveZoom = approach_f32(gPuppyCam[gCurrentMario].moveZoom, 100.0f*(gMarioState->forwardVel/32.0f), gMarioState->forwardVel/10, gMarioState->forwardVel/10);
        } else {
            gPuppyCam[gCurrentMario].moveZoom = approach_f32(gPuppyCam[gCurrentMario].moveZoom, 0, 5, 5);
        }

        // Zooms the camera in further when underwater.
        if (gPuppyCam[gCurrentMario].pitch > 0x38C0 && ABS(gPuppyCam[gCurrentMario].swimPitch) < 100) {
            gPuppyCam[gCurrentMario].zoom = approach_f32_asymptotic((f32)gPuppyCam[gCurrentMario].zoom, 250.0f, CLAMP((f32)((gPuppyCam[gCurrentMario].pitch - 0x38C0) / 3072.0f), 0.0f, 1.0f));
        }

        if (!(gMarioState->action & ACT_FLAG_SWIMMING_OR_FLYING)) {
            gPuppyCam[gCurrentMario].floorY[0] = softClamp(gPuppyCam[gCurrentMario].targetObj->oPosY - gPuppyCam[gCurrentMario].lastTargetFloorHeight, -180, 300);
            gPuppyCam[gCurrentMario].floorY[1] = softClamp(gPuppyCam[gCurrentMario].targetObj->oPosY - gPuppyCam[gCurrentMario].lastTargetFloorHeight, -180, 350);
            gPuppyCam[gCurrentMario].swimPitch = approach_f32_asymptotic(gPuppyCam[gCurrentMario].swimPitch,0,0.2f);
        } else {
            gPuppyCam[gCurrentMario].floorY[0] = 0;
            gPuppyCam[gCurrentMario].floorY[1] = 0;
            gPuppyCam[gCurrentMario].targetFloorHeight = gPuppyCam[gCurrentMario].targetObj->oPosY;
            gPuppyCam[gCurrentMario].lastTargetFloorHeight = gPuppyCam[gCurrentMario].targetObj->oPosY;

            gPuppyCam[gCurrentMario].yawTarget = approach_angle(gPuppyCam[gCurrentMario].yawTarget, (gMarioState->faceAngle[1] + 0x8000), (1000 * (gMarioState->forwardVel / 32)));
            if ((gMarioState->waterLevel - 100 - gMarioState->pos[1] > 5 && gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_PITCH_ROTATION) || gMarioState->action == ACT_FLYING || gMarioState->action == ACT_SHOT_FROM_CANNON) {
                gPuppyCam[gCurrentMario].swimPitch = approach_f32_asymptotic(gPuppyCam[gCurrentMario].swimPitch,gMarioState->faceAngle[0] / 10, 0.05f);
            } else {
                gPuppyCam[gCurrentMario].swimPitch = approach_f32_asymptotic(gPuppyCam[gCurrentMario].swimPitch, 0, 0.2f);
            }
        }

        // This sets the view offset from Mario. It accentuates a bit further when moving.
        puppycam_view_panning();

        // This sets a pseudo tilt offset based on the floor heights in front and behind mario.
        puppycam_terrain_angle();

        // This will shift the intended yaw when wall kicking, to align with the wall being kicked.
        // puppycam_wall_angle();

#ifdef ENABLE_VANILLA_LEVEL_SPECIFIC_CHECKS
 #ifdef UNLOCK_ALL
    if (gMarioState->floor != NULL && gMarioState->floor->type == SURFACE_LOOK_UP_WARP) {
 #else // !UNLOCK_ALL
    if (gMarioState->floor != NULL && gMarioState->floor->type == SURFACE_LOOK_UP_WARP
        && save_file_get_total_star_count(gCurrSaveFileNum - 1, COURSE_MIN - 1, COURSE_MAX - 1) >= 10) {
 #endif // !UNLOCK_ALL
        if (gPuppyCam[gCurrentMario].pitchTarget >= 0x7000) {
            level_trigger_warp(gMarioState, WARP_OP_LOOK_UP);
        }
    }
#endif // ENABLE_VANILLA_LEVEL_SPECIFIC_CHECKS
    } else {
        puppycam_reset_values();
    }
}

void puppycam_shake(UNUSED s16 x, UNUSED s16 y, UNUSED s16 z) {

}

/// This is the highest level of the basic steps that go into the code. Anything above is called from these following functions.

// The centrepiece behind the input side of PuppyCam. The C buttons branch off.
static void puppycam_input_core(void) {
    puppycam_analogue_stick();
    gPuppyCam[gCurrentMario].moveFlagAdd = 0;

    // Decide which input for left and right C buttons to use based on behaviour type.
    if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_INPUT_NORMAL || gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_FREE) {
        puppycam_input_hold();
    } else if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_INPUT_8DIR || gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_INPUT_4DIR) {
        puppycam_input_press();
    }
}

// Calculates the base position the camera should be, before any modification.
static void puppycam_projection(void) {
    Vec3s targetPos, targetPos2, targetPos3;
    s16 pitchTotal;
    s32 panD = (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_PANSHIFT) / 8192;

    if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_FREE) {
        puppycam_reset_values();
        puppycam_debug_view();
        return;
    }

    // Extra behaviours that get tacked onto the projection. Will be completely ignored if there is no target object.
    puppycam_projection_behaviours();
    // These are what the base rotations aspire to be.
    gPuppyCam[gCurrentMario].pitch       = CLAMP(gPuppyCam[gCurrentMario].pitch,       0x1000, 0x7000);
    gPuppyCam[gCurrentMario].pitchTarget = CLAMP(gPuppyCam[gCurrentMario].pitchTarget, 0x1000, 0x7000);
    // These are the base rotations going to be used.
    gPuppyCam[gCurrentMario].yaw   = gPuppyCam[gCurrentMario].yawTarget   - approach_f32_asymptotic((s16)(gPuppyCam[gCurrentMario].yawTarget   - gPuppyCam[gCurrentMario].yaw  ), 0, 0.3335f);
    gPuppyCam[gCurrentMario].pitch = gPuppyCam[gCurrentMario].pitchTarget - approach_f32_asymptotic((s16)(gPuppyCam[gCurrentMario].pitchTarget - gPuppyCam[gCurrentMario].pitch), 0, 0.3335f);
    // This adds the pitch effect when underwater, which is capped so it doesn't get out of control. If you're not swimming, swimpitch is 0, so it's normal.
    pitchTotal = CLAMP(gPuppyCam[gCurrentMario].pitch+(gPuppyCam[gCurrentMario].swimPitch*10)+gPuppyCam[gCurrentMario].edgePitch + gPuppyCam[gCurrentMario].terrainPitch, 800, 0x7800);

    if (gPuppyCam[gCurrentMario].targetObj) {
        vec3s_set(targetPos, gPuppyCam[gCurrentMario].targetObj->oPosX, gPuppyCam[gCurrentMario].targetObj->oPosY, gPuppyCam[gCurrentMario].targetObj->oPosZ);
        vec3s_copy(targetPos3, targetPos);
        if (gPuppyCam[gCurrentMario].targetObj2) {
            vec3s_set(targetPos2, gPuppyCam[gCurrentMario].targetObj2->oPosX, gPuppyCam[gCurrentMario].targetObj2->oPosY, gPuppyCam[gCurrentMario].targetObj2->oPosZ);
            targetPos3[0] = (s16)approach_f32_asymptotic(targetPos[0], targetPos2[0], 0.5f);
            targetPos3[1] = (s16)approach_f32_asymptotic(targetPos[1], targetPos2[1], 0.5f);
            targetPos3[2] = (s16)approach_f32_asymptotic(targetPos[2], targetPos2[2], 0.5f);
            Vec3s d;
            vec3_diff(d, targetPos, targetPos2);
            gPuppyCam[gCurrentMario].targetDist[0] = approach_f32_asymptotic(gPuppyCam[gCurrentMario].targetDist[0], (ABS(LENCOS(sqrtf(sqr(d[0]) + sqr(d[2])),
                            (s16)ABS(((gPuppyCam[gCurrentMario].yaw + 0x8000) % 0xFFFF - 0x8000) - (atan2s(d[2], d[0])) % 0xFFFF - 0x8000) + 0x4000))), 0.2f);
        } else {
            gPuppyCam[gCurrentMario].targetDist[0] = approach_f32_asymptotic(gPuppyCam[gCurrentMario].targetDist[0], 0, 0.2f);
        }

        gPuppyCam[gCurrentMario].targetDist[1] = gPuppyCam[gCurrentMario].targetDist[0] + gPuppyCam[gCurrentMario].zoom+gPuppyCam[gCurrentMario].moveZoom;

        if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_X_MOVEMENT) gPuppyCam[gCurrentMario].focus[0] = targetPos3[0] + gPuppyCam[gCurrentMario].shake[0] + (gPuppyCam[gCurrentMario].pan[0] * gPuppyCam[gCurrentMario].targetDist[1] / gPuppyCam[gCurrentMario].zoomPoints[2]) * panD;
        if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_Y_MOVEMENT) gPuppyCam[gCurrentMario].focus[1] = targetPos3[1] + gPuppyCam[gCurrentMario].shake[1] + (gPuppyCam[gCurrentMario].pan[1] * gPuppyCam[gCurrentMario].targetDist[1] / gPuppyCam[gCurrentMario].zoomPoints[2]) + gPuppyCam[gCurrentMario].povHeight - gPuppyCam[gCurrentMario].floorY[0] + (gPuppyCam[gCurrentMario].swimPitch / 10);
        if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_Z_MOVEMENT) gPuppyCam[gCurrentMario].focus[2] = targetPos3[2] + gPuppyCam[gCurrentMario].shake[2] + (gPuppyCam[gCurrentMario].pan[2] * gPuppyCam[gCurrentMario].targetDist[1] / gPuppyCam[gCurrentMario].zoomPoints[2]) * panD;

        if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_X_MOVEMENT) gPuppyCam[gCurrentMario].pos[0] = gPuppyCam[gCurrentMario].targetObj->oPosX + LENSIN(LENSIN(gPuppyCam[gCurrentMario].targetDist[1], pitchTotal), gPuppyCam[gCurrentMario].yaw) + gPuppyCam[gCurrentMario].shake[0];
        if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_Y_MOVEMENT) gPuppyCam[gCurrentMario].pos[1] = gPuppyCam[gCurrentMario].targetObj->oPosY + gPuppyCam[gCurrentMario].povHeight + LENCOS(gPuppyCam[gCurrentMario].targetDist[1], pitchTotal) + gPuppyCam[gCurrentMario].shake[1] - gPuppyCam[gCurrentMario].floorY[1];
        if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_Z_MOVEMENT) gPuppyCam[gCurrentMario].pos[2] = gPuppyCam[gCurrentMario].targetObj->oPosZ + LENCOS(LENSIN(gPuppyCam[gCurrentMario].targetDist[1], pitchTotal), gPuppyCam[gCurrentMario].yaw) + gPuppyCam[gCurrentMario].shake[2];
    }

}

// Calls any scripts to affect the camera, if applicable.
static void puppycam_script(void) {
    u16 i = 0;
    struct sPuppyVolume volume;

    if (gPuppyVolumeCount == 0 || !gPuppyCam[gCurrentMario].targetObj) {
        return;
    }
    for (i = 0; i < gPuppyVolumeCount; i++) {
        if (puppycam_check_volume_bounds(&volume, i)) {
            // First applies pos and focus, for the most basic of volumes.
            if (volume.angles != NULL) {
                if (volume.angles->pos[0]   != PUPPY_NULL) gPuppyCam[gCurrentMario].pos[0]   = volume.angles->pos[0];
                if (volume.angles->pos[1]   != PUPPY_NULL) gPuppyCam[gCurrentMario].pos[1]   = volume.angles->pos[1];
                if (volume.angles->pos[2]   != PUPPY_NULL) gPuppyCam[gCurrentMario].pos[2]   = volume.angles->pos[2];

                if (volume.angles->focus[0] != PUPPY_NULL) gPuppyCam[gCurrentMario].focus[0] = volume.angles->focus[0];
                if (volume.angles->focus[1] != PUPPY_NULL) gPuppyCam[gCurrentMario].focus[1] = volume.angles->focus[1];
                if (volume.angles->focus[2] != PUPPY_NULL) gPuppyCam[gCurrentMario].focus[2] = volume.angles->focus[2];

                if (volume.angles->yaw != PUPPY_NULL) {
                    gPuppyCam[gCurrentMario].yawTarget = volume.angles->yaw;
                    gPuppyCam[gCurrentMario].yaw       = volume.angles->yaw;

                    gPuppyCam[gCurrentMario].flags &= ~PUPPYCAM_BEHAVIOUR_YAW_ROTATION;
                }

                if (volume.angles->pitch != PUPPY_NULL) {
                    gPuppyCam[gCurrentMario].pitchTarget = volume.angles->pitch;
                    gPuppyCam[gCurrentMario].pitch       = volume.angles->pitch;

                    gPuppyCam[gCurrentMario].flags &= ~PUPPYCAM_BEHAVIOUR_PITCH_ROTATION;
                }

                if (volume.angles->zoom != PUPPY_NULL) {
                    gPuppyCam[gCurrentMario].zoomTarget = volume.angles->zoom;
                    gPuppyCam[gCurrentMario].zoom       = gPuppyCam[gCurrentMario].zoomTarget;

                    gPuppyCam[gCurrentMario].flags &= ~PUPPYCAM_BEHAVIOUR_ZOOM_CHANGE;
                }
            }

            // Adds and removes behaviour flags, as set.
            if (volume.flagsRemove) gPuppyCam[gCurrentMario].flags &= ~volume.flagsRemove;
            if (volume.flagsAdd   ) gPuppyCam[gCurrentMario].flags |=  volume.flagsAdd;
            if (volume.flagPersistance == PUPPYCAM_BEHAVIOUR_PERMANENT) {
                // Adds and removes behaviour flags, as set.
                if (volume.flagsRemove) gPuppyCam[gCurrentMario].intendedFlags &= ~volume.flagsRemove;
                if (volume.flagsAdd   ) gPuppyCam[gCurrentMario].intendedFlags |=  volume.flagsAdd;
            }

            // Last and probably least, check if there's a function attached, and call it, if so.
            if (volume.func) {
                (volume.func)();
            }
        }
    }
}

// Handles collision detection using ray casting.
static void puppycam_collision(void) {
    struct WallCollisionData wall0, wall1;
    struct Surface *surf[2];
    Vec3f camdir[2];
    Vec3f hitpos[2];
    Vec3f target[2];
    s16 pitchTotal = CLAMP(gPuppyCam[gCurrentMario].pitch+(gPuppyCam[gCurrentMario].swimPitch * 10) + gPuppyCam[gCurrentMario].edgePitch + gPuppyCam[gCurrentMario].terrainPitch, 800, 0x7800);
    s32 dist[2];

    if (gPuppyCam[gCurrentMario].targetObj == NULL) {
        return;
    }
    // The ray, starting from the top
    vec3_copy_y_off(target[0], &gPuppyCam[gCurrentMario].targetObj->oPosVec, (gPuppyCam[gCurrentMario].povHeight) - CLAMP(gPuppyCam[gCurrentMario].targetObj->oPosY - gPuppyCam[gCurrentMario].targetFloorHeight, 0, 300));
    // The ray, starting from the bottom
    vec3_copy_y_off(target[1], &gPuppyCam[gCurrentMario].targetObj->oPosVec, (gPuppyCam[gCurrentMario].povHeight * 0.4f));

    camdir[0][0] = LENSIN(LENSIN(gPuppyCam[gCurrentMario].zoomTarget, pitchTotal), gPuppyCam[gCurrentMario].yaw) + gPuppyCam[gCurrentMario].shake[0];
    camdir[0][1] = LENCOS(gPuppyCam[gCurrentMario].zoomTarget, pitchTotal) + gPuppyCam[gCurrentMario].shake[1];
    camdir[0][2] = LENCOS(LENSIN(gPuppyCam[gCurrentMario].zoomTarget, pitchTotal), gPuppyCam[gCurrentMario].yaw) + gPuppyCam[gCurrentMario].shake[2];

    vec3_copy(camdir[1], camdir[0]);

    find_surface_on_ray(target[0], camdir[0], &surf[0], hitpos[0], RAYCAST_FIND_FLOOR | RAYCAST_FIND_CEIL | RAYCAST_FIND_WALL);
    find_surface_on_ray(target[1], camdir[1], &surf[1], hitpos[1], RAYCAST_FIND_FLOOR | RAYCAST_FIND_CEIL | RAYCAST_FIND_WALL);
    resolve_and_return_wall_collisions(hitpos[0], 0.0f, 25.0f, &wall0);
    resolve_and_return_wall_collisions(hitpos[1], 0.0f, 25.0f, &wall1);
    dist[0] = ((target[0][0] - hitpos[0][0]) * (target[0][0] - hitpos[0][0]) + (target[0][1] - hitpos[0][1]) * (target[0][1] - hitpos[0][1]) + (target[0][2] - hitpos[0][2]) * (target[0][2] - hitpos[0][2]));
    dist[1] = ((target[1][0] - hitpos[1][0]) * (target[1][0] - hitpos[1][0]) + (target[1][1] - hitpos[1][1]) * (target[1][1] - hitpos[1][1]) + (target[1][2] - hitpos[1][2]) * (target[1][2] - hitpos[1][2]));

    gPuppyCam[gCurrentMario].collisionDistance = gPuppyCam[gCurrentMario].zoomTarget;

    if (surf[0] && surf[1]) {
        gPuppyCam[gCurrentMario].collisionDistance = sqrtf(MAX(dist[0], dist[1]));
        if (gPuppyCam[gCurrentMario].zoom > gPuppyCam[gCurrentMario].collisionDistance) {
            gPuppyCam[gCurrentMario].zoom = MIN(gPuppyCam[gCurrentMario].collisionDistance, gPuppyCam[gCurrentMario].zoomTarget);
            if (gPuppyCam[gCurrentMario].zoom - gPuppyCam[gCurrentMario].zoomTarget < 5) {
                if (dist[0] >= dist[1]) {
                    vec3_copy(gPuppyCam[gCurrentMario].pos, hitpos[0]);
                } else {
                    vec3_copy_y_off(gPuppyCam[gCurrentMario].pos, hitpos[1], (gPuppyCam[gCurrentMario].povHeight * 0.6f));
                }
            }
        }
    }
    #define START_DIST 500
    #define END_DIST   250
    gPuppyCam[gCurrentMario].opacity = CLAMP((f32)(((gPuppyCam[gCurrentMario].zoom - END_DIST) / 255.0f) * (START_DIST - END_DIST)), 0, 255);
}

extern Vec3f sOldPosition;
extern Vec3f sOldFocus;
extern struct PlayerGeometry sMarioGeometry[NUM_PLAYERS];

// Applies the PuppyCam values to the actual game's camera, giving the final product.
static void puppycam_apply(void) {
    vec3f_set(gLakituState[gCurrentMario].pos,       (f32)gPuppyCam[gCurrentMario].pos[0], (f32)gPuppyCam[gCurrentMario].pos[1], (f32)gPuppyCam[gCurrentMario].pos[2]);
    vec3f_set(gLakituState[gCurrentMario].goalPos,   (f32)gPuppyCam[gCurrentMario].pos[0], (f32)gPuppyCam[gCurrentMario].pos[1], (f32)gPuppyCam[gCurrentMario].pos[2]);
    vec3f_set(gLakituState[gCurrentMario].curPos,    (f32)gPuppyCam[gCurrentMario].pos[0], (f32)gPuppyCam[gCurrentMario].pos[1], (f32)gPuppyCam[gCurrentMario].pos[2]);
    vec3f_set(gCamera->pos,           (f32)gPuppyCam[gCurrentMario].pos[0], (f32)gPuppyCam[gCurrentMario].pos[1], (f32)gPuppyCam[gCurrentMario].pos[2]);
    vec3f_set(sOldPosition,           (f32)gPuppyCam[gCurrentMario].pos[0], (f32)gPuppyCam[gCurrentMario].pos[1], (f32)gPuppyCam[gCurrentMario].pos[2]);

    vec3f_set(gLakituState[gCurrentMario].focus,     (f32)gPuppyCam[gCurrentMario].focus[0], (f32)gPuppyCam[gCurrentMario].focus[1], (f32)gPuppyCam[gCurrentMario].focus[2]);
    vec3f_set(gLakituState[gCurrentMario].goalFocus, (f32)gPuppyCam[gCurrentMario].focus[0], (f32)gPuppyCam[gCurrentMario].focus[1], (f32)gPuppyCam[gCurrentMario].focus[2]);
    vec3f_set(gLakituState[gCurrentMario].curFocus,  (f32)gPuppyCam[gCurrentMario].focus[0], (f32)gPuppyCam[gCurrentMario].focus[1], (f32)gPuppyCam[gCurrentMario].focus[2]);
    vec3f_set(gCamera->focus,         (f32)gPuppyCam[gCurrentMario].focus[0], (f32)gPuppyCam[gCurrentMario].focus[1], (f32)gPuppyCam[gCurrentMario].focus[2]);
    vec3f_set(sOldFocus,              (f32)gPuppyCam[gCurrentMario].focus[0], (f32)gPuppyCam[gCurrentMario].focus[1], (f32)gPuppyCam[gCurrentMario].focus[2]);

    gCamera->yaw         = gPuppyCam[gCurrentMario].yaw;
    gCamera->nextYaw     = gPuppyCam[gCurrentMario].yaw;

    gLakituState[gCurrentMario].yaw     = gPuppyCam[gCurrentMario].yaw;
    gLakituState[gCurrentMario].nextYaw = gPuppyCam[gCurrentMario].yaw;
    gLakituState[gCurrentMario].oldYaw  = gPuppyCam[gCurrentMario].yaw;

    gLakituState[gCurrentMario].mode    = gCamera->mode;
    gLakituState[gCurrentMario].defMode = gCamera->defMode;
    gLakituState[gCurrentMario].roll    = 0;

    // Commented out simply because vanilla SM64 has this always set sometimes, and relies on certain camera modes to apply secondary foci.
    // Uncomment to have fun with certain angles.
    /*if (gSecondCameraFocus[gCurrentMario] != NULL) {
        gPuppyCam[gCurrentMario].targetObj2 = gSecondCameraFocus[gCurrentMario];
    } else {
        gPuppyCam[gCurrentMario].targetObj2 = NULL;
    }*/

    if (gMarioState->floor != NULL) {
        sMarioGeometry[gCurrentMario].currFloor       = gMarioState->floor;
        sMarioGeometry[gCurrentMario].currFloorHeight = gMarioState->floorHeight;
        sMarioGeometry[gCurrentMario].currFloorType   = gMarioState->floor->type;
    }

    if (gMarioState->ceil != NULL) {
        sMarioGeometry[gCurrentMario].currCeil        = gMarioState->ceil;
        sMarioGeometry[gCurrentMario].currCeilHeight  = gMarioState->ceilHeight;
        sMarioGeometry[gCurrentMario].currCeilType    = gMarioState->ceil->type;
    }
}

// The basic loop sequence, which is called outside.
void puppycam_loop(void) {
    if (!gPuppyCam[gCurrentMario].cutscene && sDelayedWarpOp == 0) {
        // Sets this before going through any possible modifications.
        gPuppyCam[gCurrentMario].flags = gPuppyCam[gCurrentMario].intendedFlags;
        puppycam_input_core();
        puppycam_projection();
        puppycam_script();
        if (gPuppyCam[gCurrentMario].flags & PUPPYCAM_BEHAVIOUR_COLLISION) {
            puppycam_collision();
        } else {
            gPuppyCam[gCurrentMario].opacity = 255;
        }
    } else if (gPuppyCam[gCurrentMario].cutscene) {
        gPuppyCam[gCurrentMario].opacity = 255;
        puppycam_process_cutscene();
    }
    puppycam_apply();
}

#endif