/******************************************************************************/
// Syndicate Wars Fan Expansion, source port of the classic game from Bullfrog.
/******************************************************************************/
/** @file game_sprts.c
 *     Lists of sprites used by the game.
 * @par Purpose:
 *     Defines references to heads, tails and data buffers of 2D sprites.
 * @par Comment:
 *     None.
 * @author   Tomasz Lis
 * @date     10 Feb 2024 - 02 May 2024
 * @par  Copying and copyrights:
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 */
/******************************************************************************/
#include <stdlib.h>
#include "game_sprts.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "bffile.h"
#include "bfmemory.h"
#include "bfmemut.h"
#include "bfsprite.h"
#include "bfgl.h"
#include "bfhwrend.h"

#include "frame_sprani.h"

#include "campaign.h"
#include "game_data.h"
#include "game_options.h"
#include "people.h"
#include "game.h"
#include "swlog.h"

/******************************************************************************/

struct TbSprite *pointer_sprites;
struct TbSprite *pointer_sprites_end;
ubyte *pointer_data;
ubyte *pointer_data_end;
short pointer_sprites_scale = 1;

struct TbSprite *pop1_sprites;
struct TbSprite *pop1_sprites_end;
ubyte *pop1_data;
ubyte *pop1_data_end;
short pop1_sprites_scale = 1;

struct TbSprite *small_font;
struct TbSprite *small_font_end;
ubyte *small_font_data;
ubyte *small_font_data_end;

struct TbSprite *small2_font;
struct TbSprite *small2_font_end;
ubyte *small2_font_data;

struct TbSprite *small_med_font;
struct TbSprite *small_med_font_end;
ubyte *small_med_font_data;

struct TbSprite *med_font;
struct TbSprite *med_font_end;
ubyte *med_font_data;

struct TbSprite *med2_font;
struct TbSprite *med2_font_end;
ubyte *med2_font_data;

struct TbSprite *big_font;
struct TbSprite *big_font_end;
ubyte *big_font_data;

struct TbSprite *fe_icons_sprites;
struct TbSprite *fe_icons_sprites_end;
ubyte *fe_icons_sprites_data;

struct TbSprite *wepicons_sprites;
struct TbSprite *wepicons_sprites_end;
ubyte *wepicons_sprites_data;

struct TbSprite *fe_mouseptr_sprites;
struct TbSprite *fe_mouseptr_sprites_end;
ubyte *fe_mouseptr_sprites_data;

struct TbSprite *fepanel_sprites;
struct TbSprite *fepanel_sprites_end;
ubyte *fepanel_sprites_data;

ubyte *m_sprites_data;
ubyte *m_sprites_data_end;
ubyte game_enemy_costume = 3;
static ubyte *m_sprites_alt_data = NULL;
static ubyte *m_sprites_alt_data_buf = NULL;
static ubyte *m_sprites_alt_tab_buf = NULL;
/******************************************************************************/

/** Load sprites of given style and detail, to a preallocated area of specific size.
 *
 * Loads files to areas between given start and end pointers. Expects the loaded
 * files to fit before the given end pointers.
 * For DAT file, decreases the end pointer, setting it to the end of loaded data.
 * For TAB file, clears any extra area above loaded data until the end pointer.
 *
 * The end pointers are expected to be initially set to max supported data size.
 */
static inline TbResult load_sprites_with_detail(ubyte *p_dat, ubyte **pp_dat_end,
  ubyte *p_spr, ubyte *p_spr_end, const char *dir, const char *name,
  short styleno, short detail)
{
    char locstr[DISKPATH_SIZE];
    long len;
    TbResult ret;

    sprintf(locstr, "%s/%s%hd-%hd.tab", dir, name, styleno, detail);
    if (!LbFileExists(locstr)) {
        LOGSYNC("Could not find \"%s\" file", locstr);
        LbMemorySet(p_spr, 0, p_spr_end - p_spr);
        return Lb_FAIL;
    }

    ret = Lb_SUCCESS;

    sprintf(locstr, "%s/%s%hd-%hd.dat", dir, name, styleno, detail);
    len = LbFileLoadAt(locstr, p_dat);
    if (len == -1) {
        ret = Lb_FAIL;
        len = 0;
    }
    else if (p_dat + len > (*pp_dat_end)) {
        ret = Lb_OK;
        LOGERR("Load caused overflow by %d bytes, file \"%s\"", p_dat + len - (*pp_dat_end), locstr);
    }
    else {
        *pp_dat_end = p_dat + len;
    }

    sprintf(locstr, "%s/%s%hd-%hd.tab", dir, name, styleno, detail);
    len = LbFileLoadAt(locstr, p_spr);
    if (len == -1) {
        ret = Lb_FAIL;
        LbMemorySet(p_spr, 0, p_spr_end - p_spr);
    }
    else if (p_spr + len > p_spr_end) {
        if (ret == Lb_SUCCESS) ret = Lb_OK;
        LOGERR("Load caused overflow by %d bytes, file \"%s\"", p_spr + len - p_spr_end, locstr);
        // Clear the overflown data
        LbMemorySet(p_spr + len, 0, p_spr + len - p_spr_end);
    }
    else if (p_spr + len < p_spr_end) {
        if (ret == Lb_SUCCESS) ret = Lb_OK;
        // If loaded file is smaller than allocated space, clear the rest; we do not access
        // data pointer for zero-sized sprites, so this is enough as safety measure
        LbMemorySet(p_spr + len, 0, p_spr_end - p_spr - len);
    }

    return ret;
}

static inline TbResult load_any_sprites_up_to(const char *dir, const char *name,
  ushort min_sprites, struct TbSprite *p_sprites, struct TbSprite *p_sprites_end,
  ubyte *p_data, ubyte **pp_data_end, short *p_scale, short styleno, short max_detail)
{
    short detail;
    TbResult ret;

    if (p_sprites_end - p_sprites < min_sprites) {
        LOGERR("Preallocated area for %d '%s%hd' sprites is below expected minimum %d",
         p_sprites_end - p_sprites, name, styleno, min_sprites);
    }
    ret = Lb_FAIL;
    for (detail = max_detail; detail >= 0; detail--)
    {
        ubyte *loc_data_end;

        loc_data_end = *pp_data_end;
        ret = load_sprites_with_detail(p_data, &loc_data_end,
          (ubyte *)p_sprites, (ubyte *)p_sprites_end,
          dir, name, styleno, detail);
        if (ret != Lb_FAIL) {
            *pp_data_end = loc_data_end;
            break;
        }
    }
    if (detail < 0) {
        LOGERR("Some '%s%hd' sprites not loaded, tried detail %hd..0",
          name, styleno, max_detail);
        detail = 0;
    }
    if (p_scale != NULL)
        *p_scale = detail + 1;

    return ret;
}

TbResult load_sprites_mouse_pointers_up_to(const char *dir, short styleno, short max_detail)
{
    ubyte *loc_data_end = pointer_data_end;
    return load_any_sprites_up_to(dir, "pointr", 10, pointer_sprites, pointer_sprites_end,
      pointer_data, &loc_data_end, &pointer_sprites_scale, styleno, max_detail);
}

void setup_mouse_pointers(void)
{
    struct TbSprite *spr;

    if ((pointer_sprites == NULL) || (pointer_data == NULL)) {
        LOGERR("Pointer sprite memory not allocated, skipping");
        return;
    }
    LbSpriteSetup(pointer_sprites, pointer_sprites_end, pointer_data);
    // Make mouse pointer sprite 1 an empty (zero size) sprite
    spr = &pointer_sprites[1];
    spr->SWidth = 0;
    spr->SHeight = 0;
}

void reset_mouse_pointers(void)
{
    LbSpriteReset(pointer_sprites, pointer_sprites_end, pointer_data);
}

TbResult load_sprites_fe_icons(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail)
{
    ubyte *p_buf;
    ushort min_sprites;
    TbResult ret;

    min_sprites = 170;
    p_buf = *pp_buf;
    fe_icons_sprites = (struct TbSprite *)p_buf;
    p_buf += min_sprites * sizeof(struct TbSprite);
    fe_icons_sprites_end = (struct TbSprite *)p_buf;
    fe_icons_sprites_data = p_buf;
    p_buf += min_sprites * 4096 * (max_detail + 1) * (max_detail + 1);

    ret = load_any_sprites_up_to(dir, "icons", min_sprites,
      fe_icons_sprites, fe_icons_sprites_end,
      fe_icons_sprites_data, &p_buf, NULL, styleno, max_detail);

    if (ret != Lb_FAIL)
        *pp_buf = p_buf;

    return ret;
}

void setup_sprites_fe_icons(void)
{
    LbSpriteSetup(fe_icons_sprites, fe_icons_sprites_end, fe_icons_sprites_data);
}

void reset_sprites_fe_icons(void)
{
    LbSpriteReset(fe_icons_sprites, fe_icons_sprites_end, fe_icons_sprites_data);
}

TbResult load_sprites_wepicons(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail)
{
    ubyte *p_buf;
    ushort min_sprites;
    TbResult ret;

    min_sprites = 34;
    p_buf = *pp_buf;
    wepicons_sprites = (struct TbSprite *)p_buf;
    p_buf += min_sprites * sizeof(struct TbSprite);
    wepicons_sprites_end = (struct TbSprite *)p_buf;
    wepicons_sprites_data = p_buf;
    p_buf += min_sprites * 4096 * (max_detail + 1) * (max_detail + 1);

    ret = load_any_sprites_up_to(dir, "wepicon", min_sprites,
      wepicons_sprites, wepicons_sprites_end,
      wepicons_sprites_data, &p_buf, NULL, styleno, max_detail);

    if (ret != Lb_FAIL)
        *pp_buf = p_buf;

    return ret;
}

void setup_sprites_wepicons(void)
{
    LbSpriteSetup(wepicons_sprites, wepicons_sprites_end, wepicons_sprites_data);
}

void reset_sprites_wepicons(void)
{
    LbSpriteReset(wepicons_sprites, wepicons_sprites_end, wepicons_sprites_data);
}

TbResult load_sprites_fe_mouse_pointers(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail)
{
    ubyte *p_buf;
    ushort min_sprites;
    TbResult ret;

    min_sprites = 15;
    p_buf = *pp_buf;
    fe_mouseptr_sprites = (struct TbSprite *)p_buf;
    p_buf += min_sprites * sizeof(struct TbSprite);
    fe_mouseptr_sprites_end = (struct TbSprite *)p_buf;
    fe_mouseptr_sprites_data = p_buf;
    p_buf += min_sprites * 4096 * (max_detail + 1) * (max_detail + 1);

    ret = load_any_sprites_up_to(dir, "mouse", min_sprites,
      fe_mouseptr_sprites, fe_mouseptr_sprites_end,
      fe_mouseptr_sprites_data, &p_buf, NULL, styleno, max_detail);

    if (ret != Lb_FAIL) {
        *pp_buf = p_buf;
    } else { // If load failed, still allocate the area for TAB file - the buffer is zero-filled
        *pp_buf = fe_mouseptr_sprites_data;
    }

    return ret;
}

void setup_sprites_fe_mouse_pointers(void)
{
    LbSpriteSetup(fe_mouseptr_sprites, fe_mouseptr_sprites_end, fe_mouseptr_sprites_data);
}

void reset_sprites_fe_mouse_pointers(void)
{
    LbSpriteReset(fe_mouseptr_sprites, fe_mouseptr_sprites_end, fe_mouseptr_sprites_data);
}

TbResult load_sprites_med_font(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail)
{
    ubyte *p_buf;
    ushort min_sprites;
    TbResult ret;

    min_sprites = 224;
    p_buf = *pp_buf;
    med_font = (struct TbSprite *)p_buf;
    p_buf += min_sprites * sizeof(struct TbSprite);
    med_font_end = (struct TbSprite *)p_buf;
    med_font_data = p_buf;
    p_buf += min_sprites * 4096 * (max_detail + 1) * (max_detail + 1);

    ret = load_any_sprites_up_to(dir, "fontc", min_sprites, med_font, med_font_end,
      med_font_data, &p_buf, NULL, styleno, max_detail);

    if (ret != Lb_FAIL)
        *pp_buf = p_buf;

    return ret;
}

void setup_sprites_med_font(void)
{
    LbSpriteSetup(med_font, med_font_end, med_font_data);
}

void reset_sprites_med_font(void)
{
    LbSpriteReset(med_font, med_font_end, med_font_data);
}

TbResult load_sprites_big_font(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail)
{
    ubyte *p_buf;
    ushort min_sprites;
    TbResult ret;

    min_sprites = 224;
    p_buf = *pp_buf;
    big_font = (struct TbSprite *)p_buf;
    p_buf += min_sprites * sizeof(struct TbSprite);
    big_font_end = (struct TbSprite *)p_buf;
    big_font_data = p_buf;
    p_buf += min_sprites * 4096 * (max_detail + 1) * (max_detail + 1);

    ret = load_any_sprites_up_to(dir, "fontc", min_sprites, big_font, big_font_end,
      big_font_data, &p_buf, NULL, styleno, max_detail);

    if (ret != Lb_FAIL)
        *pp_buf = p_buf;

    return ret;
}

void setup_sprites_big_font(void)
{
    LbSpriteSetup(big_font, big_font_end, big_font_data);
}

void reset_sprites_big_font(void)
{
    LbSpriteReset(big_font, big_font_end, big_font_data);
}

TbResult load_sprites_small_font_up_to(const char *dir, short max_detail)
{
    ubyte *loc_data_end = small_font_data_end;
    const short styleno = 0;
    return load_any_sprites_up_to(dir, "fontc", 224, small_font, small_font_end,
      small_font_data, &loc_data_end, NULL, styleno, max_detail);
}

void setup_sprites_small_font(void)
{
    LbSpriteSetup(small_font, small_font_end, small_font_data);
}

void reset_sprites_small_font(void)
{
    LbSpriteReset(small_font, small_font_end, small_font_data);
}

TbResult load_sprites_small_med_font(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail)
{
    ubyte *p_buf;
    ushort min_sprites;
    TbResult ret;

    min_sprites = 224;
    p_buf = *pp_buf;
    small_med_font = (struct TbSprite *)p_buf;
    p_buf += min_sprites * sizeof(struct TbSprite);
    small_med_font_end = (struct TbSprite *)p_buf;
    small_med_font_data = p_buf;
    p_buf += min_sprites * 4096 * (max_detail + 1) * (max_detail + 1);

    ret = load_any_sprites_up_to(dir, "fontc", min_sprites, small_med_font, small_med_font_end,
      small_med_font_data, &p_buf, NULL, styleno, max_detail);

    if (ret != Lb_FAIL)
        *pp_buf = p_buf;

    return ret;
}

void setup_sprites_small_med_font(void)
{
    LbSpriteSetup(small_med_font, small_med_font_end, small_med_font_data);
}

void reset_sprites_small_med_font(void)
{
    LbSpriteReset(small_med_font, small_med_font_end, small_med_font_data);
}

TbResult load_sprites_med2_font(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail)
{
    ubyte *p_buf;
    ushort min_sprites;
    TbResult ret;

    min_sprites = 224;
    p_buf = *pp_buf;
    med2_font = (struct TbSprite *)p_buf;
    p_buf += min_sprites * sizeof(struct TbSprite);
    med2_font_end = (struct TbSprite *)p_buf;
    med2_font_data = p_buf;
    p_buf += min_sprites * 4096 * (max_detail + 1) * (max_detail + 1);

    ret = load_any_sprites_up_to(dir, "fontc", min_sprites, med2_font, med2_font_end,
      med2_font_data, &p_buf, NULL, styleno, max_detail);

    if (ret != Lb_FAIL)
        *pp_buf = p_buf;

    return ret;
}

void setup_sprites_med2_font(void)
{
    LbSpriteSetup(med2_font, med2_font_end, med2_font_data);
}

void reset_sprites_med2_font(void)
{
    LbSpriteReset(med2_font, med2_font_end, med2_font_data);
}

TbResult load_sprites_small2_font(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail)
{
    ubyte *p_buf;
    ushort min_sprites;
    TbResult ret;

    min_sprites = 224;
    p_buf = *pp_buf;
    small2_font = (struct TbSprite *)p_buf;
    p_buf += min_sprites * sizeof(struct TbSprite);
    small2_font_end = (struct TbSprite *)p_buf;
    small2_font_data = p_buf;
    p_buf += min_sprites * 4096 * (max_detail + 1) * (max_detail + 1);

    ret = load_any_sprites_up_to(dir, "fontc", min_sprites, small2_font, small2_font_end,
      small2_font_data, &p_buf, NULL, styleno, max_detail);

    if (ret != Lb_FAIL)
        *pp_buf = p_buf;

    return ret;
}

void setup_sprites_small2_font(void)
{
    LbSpriteSetup(small2_font, small2_font_end, small2_font_data);
}

void reset_sprites_small2_font(void)
{
    LbSpriteReset(small2_font, small2_font_end, small2_font_data);
}

/** One part of a team's costume which can get another colour. The original colour of the part is a set of shades
 * in the palette; the shades are put in bands by their brightness, and each band has a marker, which the GPU
 * shows in the chosen colour. The first shade of a band, its brightest, is the entry point of the lighting. */
struct CustomCostumePart {
    /** The palette indices of the shades of the part, and how many. */
    const ubyte *indices;
    ubyte count;
    /** The number of bands, each with its own marker. */
    ubyte bands;
    /** The shade (palette index) which gets exactly the chosen colour. */
    ubyte ref;
    /** Palette indices which the sprites never use; the pixels of the replaced shades get these, and the GPU shows
     * them in the chosen colour. One for each band. */
    const ubyte *markers;
    const char *name;
};

/* The shades of the parts. The Eurocorp agents: the greenish trench coat and the yellow shoulders of costume 3 */
static const ubyte eur_coat_shades[12] = { 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47 };
static const ubyte eur_shoulder_shades[8] = { 167, 168, 169, 170, 171, 172, 173, 174 };
/* The Epoch members: the white cloak, from white to dark blue-grey, with the darkest shades of it which show when it
 * faces away; and the robe, which is the red of the hood and sleeves and the brown which goes with it */
static const ubyte epoch_cloak_shades[28] = {
    80, 81, 82, 83, 84, 85, 86, 87, 88, 89, 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103,
    104, 105, 106, 107 };
static const ubyte epoch_robe_shades[32] = {
    120, 121, 122, 123, 124, 125, 126, 127,
    136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151,
    152, 153, 154, 155, 156, 157, 158, 159 };

/* The markers. The teams are not replaced together, so both can use the same ones */
static const ubyte custom_markers_a[12] = { 224, 225, 226, 227, 228, 229, 230, 231, 26, 28, 112, 246 };
static const ubyte custom_markers_b[10] = { 176, 177, 178, 179, 208, 209, 210, 253, 26, 28 };
static const ubyte custom_markers_c[10] = { 224, 225, 226, 227, 228, 229, 230, 231, 112, 246 };

static const struct CustomCostumePart custom_costume_parts[CUSTOM_TEAMS][CUSTOM_COSTUME_PARTS] = {
    {
        { eur_coat_shades, 12, 12, 41, custom_markers_a, "TRENCH COAT" },
        { eur_shoulder_shades, 8, 8, 170, custom_markers_b, "SHOULDERS" },
    },
    {
        { epoch_cloak_shades, 28, 10, 90, custom_markers_c, "CLOAK" },
        { epoch_robe_shades, 32, 10, 124, custom_markers_b, "ROBE" },
    },
};

TbBool game_custom_used_all[CUSTOM_TEAMS][CUSTOM_COSTUME_PARTS];
ubyte game_custom_rgb_all[CUSTOM_TEAMS][CUSTOM_COSTUME_PARTS][3];

static ubyte custom_costume_pal[768];
static TbBool custom_costume_pal_loaded = false;

/** How the shades of each part are put in bands; made once the palette is known. */
static struct {
    /** The band of each palette index, or 0xFF if it is not a shade of the part. */
    ubyte band_of[256];
    /** The brightest shade of the band, and the brightness which the band has on the average. */
    ubyte band_first[16];
    int band_lum[16];
} custom_costume_bands[CUSTOM_TEAMS][CUSTOM_COSTUME_PARTS];
static TbBool custom_costume_bands_made = false;

static int custom_costume_lum(int idx)
{
    const ubyte *c = &custom_costume_pal[idx * 3];
    return c[0] * 30 + c[1] * 59 + c[2] * 11;
}

static void custom_costume_pal_load(void)
{
    int team, part, i, j;

    if (custom_costume_pal_loaded)
        return;
    // The palette without brightness changes; 6 bits per channel
    if (LbFileLoadAt("qdata/pal0.dat", custom_costume_pal) == -1)
        LbMemorySet(custom_costume_pal, '\0', sizeof(custom_costume_pal));
    custom_costume_pal_loaded = true;

    // The shades of each part, from the brightest, in bands of the same number of shades
    if (custom_costume_bands_made)
        return;
    for (team = 0; team < CUSTOM_TEAMS; team++)
    {
        for (part = 0; part < CUSTOM_COSTUME_PARTS; part++)
        {
            const struct CustomCostumePart *p_part = &custom_costume_parts[team][part];
            ubyte order[64];
            int nband[16];

            memset(custom_costume_bands[team][part].band_of, 0xFF, 256);
            memset(nband, 0, sizeof(nband));
            for (i = 0; i < p_part->count; i++)
                order[i] = p_part->indices[i];
            for (i = 1; i < p_part->count; i++)
            {
                ubyte key = order[i];

                for (j = i - 1; (j >= 0) && (custom_costume_lum(order[j]) < custom_costume_lum(key)); j--)
                    order[j + 1] = order[j];
                order[j + 1] = key;
            }
            for (i = 0; i < p_part->bands; i++)
            {
                custom_costume_bands[team][part].band_lum[i] = 0;
                custom_costume_bands[team][part].band_first[i] = order[0];
            }
            for (i = 0; i < p_part->count; i++)
            {
                int band = i * p_part->bands / p_part->count;

                custom_costume_bands[team][part].band_of[order[i]] = band;
                if (nband[band] == 0)
                    custom_costume_bands[team][part].band_first[band] = order[i];
                custom_costume_bands[team][part].band_lum[band] += custom_costume_lum(order[i]);
                nband[band]++;
            }
            for (i = 0; i < p_part->bands; i++)
            {
                if (nband[i] > 0)
                    custom_costume_bands[team][part].band_lum[i] /= nband[i];
            }
        }
    }
    custom_costume_bands_made = true;
}

int game_custom_team(void)
{
    // The Church campaign is played with the Epoch members; the others with the agents
    return (background_type == 1) ? CUSTOM_TEAM_EPOCH : CUSTOM_TEAM_EUROCORP;
}

static const struct CustomCostumePart *custom_costume_part(int part)
{
    return &custom_costume_parts[game_custom_team()][part];
}

const char *game_custom_costume_part_name(int part)
{
    return custom_costume_part(part)->name;
}

int game_custom_costume_shades(int part)
{
    return custom_costume_part(part)->bands;
}

ubyte game_custom_costume_original(int part, int shade)
{
    custom_costume_pal_load();
    return custom_costume_bands[game_custom_team()][part].band_first[shade];
}

int game_custom_costume_swatch(int idx)
{
    int s;

    // The screen shows the marker indices in the chosen colours; a swatch of the palette
    // takes the colour of the index below instead, which is close
    for (;;)
    {
        for (s = 0; s < 12; s++)
            if (custom_markers_a[s] == idx)
                goto is_marker;
        for (s = 0; s < 10; s++)
            if (custom_markers_b[s] == idx)
                goto is_marker;
        return idx;
is_marker:
        if (idx == 0)
            return idx;
        idx--;
    }
}

ubyte game_custom_costume_marker(int part, int shade)
{
    return custom_costume_part(part)->markers[shade];
}

TbBool game_custom_costume_hw(void)
{
    return LbGlIsActive() && LbHwRenderEnabled();
}

void game_custom_costume_default_rgb(int part, ubyte *rgb)
{
    int i;

    custom_costume_pal_load();
    for (i = 0; i < 3; i++)
        rgb[i] = custom_costume_pal[custom_costume_part(part)->ref * 3 + i] * 255 / 63;
}

void game_custom_costume_palette_rgb(int idx, ubyte *rgb)
{
    int i;

    custom_costume_pal_load();
    for (i = 0; i < 3; i++)
        rgb[i] = custom_costume_pal[idx * 3 + i] * 255 / 63;
}

TbBool game_custom_costume_active(void)
{
    int part;

    if (!game_custom_costume_hw())
        return false;
    for (part = 0; part < CUSTOM_COSTUME_PARTS; part++)
        if (game_custom_used[part])
            return true;
    return false;
}

void game_custom_costume_publish(void)
{
    ubyte markers[HW_CUSTOM_MAX], orig[HW_CUSTOM_MAX], rgb[HW_CUSTOM_MAX * 3];
    int part, s, n, i, team;

    n = 0;
    team = game_custom_team();
    custom_costume_pal_load();
    for (part = 0; part < CUSTOM_COSTUME_PARTS; part++)
    {
        const struct CustomCostumePart *p_part = custom_costume_part(part);
        int lref;

        if (!game_custom_used[part] || !game_custom_costume_hw())
            continue;
        lref = custom_costume_lum(p_part->ref);
        if (lref < 1)
            lref = 1;
        for (s = 0; s < p_part->bands; s++)
        {
            int lo;

            // The entry point of the lighting is the brightest shade of the band, and the brightness of
            // the band is the average of its shades
            lo = custom_costume_bands[team][part].band_lum[s];
            markers[n] = p_part->markers[s];
            orig[n] = custom_costume_bands[team][part].band_first[s];
            // The chosen colour for the reference shade, darker or brighter for the other shades
            for (i = 0; i < 3; i++)
            {
                int v = game_custom_rgb[part][i] * lo / lref;
                rgb[n * 3 + i] = (v > 255) ? 255 : v;
            }
            n++;
        }
    }
    LbHwCustomColoursSet(n, markers, orig, rgb);
}

/** Fills the table which says what a shade of the replaced colours becomes: its marker. */
static void custom_costume_make_lut(ubyte *lut)
{
    int part, i, team;

    team = game_custom_team();
    custom_costume_pal_load();
    for (i = 0; i < 256; i++)
        lut[i] = i;
    for (part = 0; part < CUSTOM_COSTUME_PARTS; part++)
    {
        const struct CustomCostumePart *p_part = custom_costume_part(part);

        if (!game_custom_used[part])
            continue;
        for (i = 0; i < p_part->count; i++)
        {
            int idx = p_part->indices[i];

            lut[idx] = p_part->markers[custom_costume_bands[team][part].band_of[idx]];
        }
    }
}

/** Replaces the colours of the Eurocorp costume in the loaded sprites.
 *
 * Only the pixels which differ from the first costume are changed, as the same shades are used by
 * other people in other places. The sprites still have offsets in their data pointers.
 */
static void custom_costume_apply(const char *dir, ubyte *dat, long dat_len, struct TbSprite *tab, long tab_len)
{
    char locstr[DISKPATH_SIZE];
    ubyte lut[256];
    ubyte *orig, *ref_dat;
    struct TbSprite *ref_tab;
    long ref_dat_len, ref_tab_len, n, i;

    if (!game_custom_costume_active() || (dat_len <= 0) || (tab_len <= 0))
        return;
    custom_costume_make_lut(lut);

    orig = (ubyte *)malloc(dat_len);
    ref_dat = (ubyte *)malloc((m_sprites_data_end - m_sprites_data) + 1024);
    ref_tab = (struct TbSprite *)malloc(64 * 1024);
    if ((orig == NULL) || (ref_dat == NULL) || (ref_tab == NULL))
        goto done;
    sprintf(locstr, "%s/mspr-%d.dat", dir, 0);
    ref_dat_len = LbFileLoadAt(locstr, ref_dat);
    sprintf(locstr, "%s/mspr-%d.tab", dir, 0);
    ref_tab_len = LbFileLoadAt(locstr, ref_tab);
    if ((ref_dat_len == -1) || (ref_tab_len == -1))
        goto done;
    // Sprites can share their data; the colours are taken from the untouched copy, so each pixel is changed once
    memcpy(orig, dat, dat_len);

    n = tab_len / sizeof(struct TbSprite);
    if (n > ref_tab_len / (long)sizeof(struct TbSprite))
        n = ref_tab_len / sizeof(struct TbSprite);
    for (i = 0; i < n; i++)
    {
        long p, q;
        int y, aligned;

        p = (long)(ulong)tab[i].Data;
        q = (long)(ulong)ref_tab[i].Data;
        if ((tab[i].SWidth == 0) || (tab[i].SHeight == 0) || (p >= dat_len) || (q >= ref_dat_len))
            continue;
        aligned = 1;
        for (y = 0; (y < tab[i].SHeight) && aligned; y++)
        {
            // Lines of: n > 0 - n pixels follow; n < 0 - skip n pixels; 0 - end of line
            while ((p < dat_len) && (q < ref_dat_len))
            {
                int c, c0, k;

                c = (signed char)orig[p++];
                c0 = (signed char)ref_dat[q++];
                if (c != c0) {
                    aligned = 0; // the other costume has other shape; leave the rest of the sprite
                    break;
                }
                if (c == 0)
                    break;
                if (c < 0)
                    continue;
                for (k = 0; (k < c) && (p + k < dat_len) && (q + k < ref_dat_len); k++)
                {
                    if (orig[p + k] != ref_dat[q + k])
                        dat[p + k] = lut[orig[p + k]];
                }
                p += c;
                q += c;
            }
        }
    }
done:
    free(orig);
    free(ref_dat);
    free(ref_tab);
}

/** Replaces the colours of the Epoch members (the zealots) in the loaded sprites.
 *
 * The zealots look the same in all the costume files, so there is no other costume to compare with; the
 * sprites of the zealots are found by the frames which draw them, and only these are changed.
 */
static void custom_costume_apply_zealots(ubyte *dat, long dat_len, struct TbSprite *tab, long tab_len)
{
    ubyte lut[256];
    ubyte *mask;
    long n, i;
    int anim, ang;

    if (!game_custom_costume_active() || (dat_len <= 0) || (tab_len <= 0))
        return;
    n = tab_len / sizeof(struct TbSprite);
    mask = (ubyte *)calloc(n, 1);
    if (mask == NULL)
        return;
    for (anim = 0; anim < ANIM_PERS_TOTAL_COUNT; anim++)
    {
        for (ang = 0; ang < 12; ang++)
        {
            ushort frm, first_frm;
            int guard;

            if (&nstart_ani[people_frames[SubTT_PERS_ZEALOT][anim] + ang] >= nstart_ani_end)
                break;
            frm = nstart_ani[people_frames[SubTT_PERS_ZEALOT][anim] + ang];
            first_frm = frm;
            // The frames of the animation, one after another
            for (guard = 0; (guard < 64) && (&frame[frm] < frame_end); guard++)
            {
                ushort el;
                int eguard;

                for (el = frame[frm].FirstElement, eguard = 0; (el > 0) && (eguard < 64) && (&melement_ani[el] < mele_ani_end);
                  el = melement_ani[el].Next, eguard++)
                {
                    long si = melement_ani[el].ToSprite / sizeof(struct TbSprite);

                    if ((si > 0) && (si < n))
                        mask[si] = 1;
                }
                frm = frame[frm].Next;
                if ((frm == first_frm) || (frm == 0))
                    break;
            }
        }
    }

    custom_costume_make_lut(lut);
    // Each pixel of those sprites is mapped; the markers are not in the range of the replaced shades, so
    // sprites which share their data can be mapped again without harm
    for (i = 0; i < n; i++)
    {
        long p;
        int y;

        if (!mask[i])
            continue;
        p = (long)(ulong)tab[i].Data;
        if ((tab[i].SWidth == 0) || (tab[i].SHeight == 0) || (p >= dat_len))
            continue;
        for (y = 0; y < tab[i].SHeight; y++)
        {
            // Lines of: n > 0 - n pixels follow; n < 0 - skip n pixels; 0 - end of line
            while (p < dat_len)
            {
                int c, k;

                c = (signed char)dat[p++];
                if (c == 0)
                    break;
                if (c < 0)
                    continue;
                for (k = 0; (k < c) && (p + k < dat_len); k++)
                    dat[p + k] = lut[dat[p + k]];
                p += c;
            }
        }
    }
    free(mask);
}

TbResult load_multicolor_sprites(const char *dir)
{
    char locstr[DISKPATH_SIZE];
    long len, dat_len;
    TbResult ret;

    ret = Lb_OK;

    sprintf(locstr, "%s/mspr-%d.dat", dir, ingame.TrenchcoatPreference);
    len = LbFileLoadAt(locstr, m_sprites_data);
    if (len == -1) {
        ret = Lb_FAIL;
        len = 0;
    }
    dat_len = len;
    // additional 512 bytes are always reserved by LbDataLoad()
    assert(m_sprites_data_end + 512 >= m_sprites_data + len);
    sprintf(locstr, "%s/mspr-%d.tab", dir, ingame.TrenchcoatPreference);
    len = LbFileLoadAt(locstr, m_sprites);
    if (len == -1) {
        ret = Lb_FAIL;
        len = 128 * sizeof(struct TbSprite);
        LbMemorySet(m_sprites, '\0', len);
    }
    m_sprites_end = (struct TbSprite *)((ubyte *)m_sprites + len);

    if (ret == Lb_OK) {
        // The Epoch members look the same in all the costumes; the agents have the colours only in costume 3
        if (game_custom_team() == CUSTOM_TEAM_EPOCH)
            custom_costume_apply_zealots(m_sprites_data, dat_len, m_sprites, len);
        else if (ingame.TrenchcoatPreference == CUSTOM_COSTUME_BASE)
            custom_costume_apply(dir, m_sprites_data, dat_len, m_sprites, len);
    }
    // The GPU has the colours for the markers only while the sprites have them
    if (((game_custom_team() == CUSTOM_TEAM_EPOCH) || (ingame.TrenchcoatPreference == CUSTOM_COSTUME_BASE))
      && game_custom_costume_active())
        game_custom_costume_publish();
    else
        LbHwCustomColoursSet(0, NULL, NULL, NULL);

    return ret;
}

TbResult load_multicolor_sprites_alt(const char *dir)
{
    char locstr[DISKPATH_SIZE];
    long len, data_size;
    TbResult ret;

    ret = Lb_OK;
    data_size = (m_sprites_data_end - m_sprites_data) + 1024;
    if (m_sprites_alt_data_buf == NULL) {
        m_sprites_alt_data_buf = (ubyte *)malloc(data_size);
        m_sprites_alt_tab_buf = (ubyte *)malloc(64 * 1024);
    }
    if ((m_sprites_alt_data_buf == NULL) || (m_sprites_alt_tab_buf == NULL)) {
        m_sprites_alt = NULL;
        return Lb_FAIL;
    }
    m_sprites_alt_data = m_sprites_alt_data_buf;
    sprintf(locstr, "%s/mspr-%d.dat", dir, (int)game_enemy_costume);
    len = LbFileLoadAt(locstr, m_sprites_alt_data);
    if (len == -1) {
        m_sprites_alt = NULL;
        return Lb_FAIL;
    }
    sprintf(locstr, "%s/mspr-%d.tab", dir, (int)game_enemy_costume);
    len = LbFileLoadAt(locstr, m_sprites_alt_tab_buf);
    if (len == -1) {
        m_sprites_alt = NULL;
        return Lb_FAIL;
    }
    m_sprites_alt = (struct TbSprite *)m_sprites_alt_tab_buf;
    m_sprites_alt_end = (struct TbSprite *)(m_sprites_alt_tab_buf + len);
    LbSpriteSetup(m_sprites_alt, m_sprites_alt_end, m_sprites_alt_data);
    return ret;
}

void setup_multicolor_sprites(void)
{
    LbSpriteSetup(m_sprites, m_sprites_end, m_sprites_data);
}

void reset_multicolor_sprites(void)
{
    LbSpriteReset(m_sprites, m_sprites_end, m_sprites_data);
}

void debug_multicolor_sprite(int idx)
{
    int i;
    char strdata[100];
    char *str;
    struct TbSprite *spr;
    unsigned char *ptr;

    spr = &m_sprites[idx];
    str = strdata;
    sprintf(str, "spr %d width %d height %d ptr 0x%lx data",
      idx, (int)spr->SWidth, (int)spr->SHeight, (ulong)spr->Data);
    ptr = spr->Data;
    for (i = 0; i < 10; i++)
    {
        str = strdata + strlen(strdata);
        sprintf(str, " %02x", (int)*ptr);
        ptr++;
    }
    LOGDBG("m_sprites: %s", strdata);
}

TbResult load_sprites_fepanel(ubyte **pp_buf,
  const char *dir, short styleno, short max_detail)
{
    ubyte *p_buf;
    ushort min_sprites;
    TbResult ret;

    min_sprites = 150;
    p_buf = *pp_buf;
    fepanel_sprites = (struct TbSprite *)p_buf;
    p_buf += min_sprites * sizeof(struct TbSprite);
    fepanel_sprites_end = (struct TbSprite *)p_buf;
    fepanel_sprites_data = p_buf;
    p_buf += min_sprites * 4096 * (max_detail + 1) * (max_detail + 1);

    ret = load_any_sprites_up_to(dir, "panel", min_sprites, fepanel_sprites, fepanel_sprites_end,
      fepanel_sprites_data, &p_buf, NULL, styleno, max_detail);

    if (ret != Lb_FAIL)
        *pp_buf = p_buf;

    return ret;
}

void setup_sprites_fepanel(void)
{
    LbSpriteSetup(fepanel_sprites, fepanel_sprites_end, fepanel_sprites_data);
}

void reset_sprites_fepanel(void)
{
    LbSpriteReset(fepanel_sprites, fepanel_sprites_end, fepanel_sprites_data);
}

TbResult load_pop_sprites_up_to(const char *dir, const char *name, short styleno, short max_detail)
{
    ubyte *loc_data_end = pop1_data_end;
    return load_any_sprites_up_to(dir, name, 150, pop1_sprites, pop1_sprites_end,
      pop1_data, &loc_data_end, &pop1_sprites_scale, styleno, max_detail);
}

void setup_pop_sprites(void)
{
    LbSpriteSetup(pop1_sprites, pop1_sprites_end, pop1_data);
}

void reset_pop_sprites(void)
{
    LbSpriteReset(pop1_sprites, pop1_sprites_end, pop1_data);
}

/******************************************************************************/
