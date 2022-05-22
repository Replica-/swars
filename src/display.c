#include <stdint.h>
#include <stdio.h>
#include <assert.h>

#include "display.h"
#include "bfscreen.h"
#include "bfscrsurf.h"
#include "bfpalette.h"
#include "bftext.h"
#include "bfmouse.h"
#include "util.h"
#include "bflib_basics.h"

#pragma pack(1)

#define to_SDLSurf(h) ((SDL_Surface  *)h)

extern TbScreenModeInfo lbScreenModeInfo[];

#pragma pack()

extern char lbDrawAreaTitle[128];

static bool         display_lowres_stretch = false;
static unsigned char *display_stretch_buffer = NULL;

#if defined(WIN32)

const char * SWResourceMapping(short index)
{
    switch (index)
    {
    case 1:
        return "A";
        //return MAKEINTRESOURCE(110); -- may work for other resource compilers
    default:
        return NULL;
    }
}

#else

const char * SWResourceMapping(short index)
{
    switch (index)
    {
    case 1:
        return "swars_icon.png";
    default:
        return NULL;
    }
}

#endif

static inline void
lock_screen (void)
{
  if (!SDL_MUSTLOCK (to_SDLSurf(lbScreenSurface)))
    return;

  if (SDL_LockSurface (to_SDLSurf(lbScreenSurface)) != 0)
    fprintf (stderr, "SDL_LockSurface: %s\n", SDL_GetError ());
}

static inline void
unlock_screen (void)
{
  if (!SDL_MUSTLOCK (to_SDLSurf(lbScreenSurface)))
    return;

  SDL_UnlockSurface (to_SDLSurf(lbScreenSurface));
}

int LbScreenSetupAnyModeTweaked(unsigned short mode, unsigned long width,
    unsigned long height, TbPixel *palette)
{
    ubyte *wscreen_bak;

    wscreen_bak = lbDisplay.WScreen;

    uint32_t flags;
    TbScreenModeInfo *mdinfo;

    // OLD: should we really suspend/resume the mouse here?
    long hot_x, hot_y;
    const struct TbSprite *msspr;

    msspr = NULL;
    if (lbDisplay.MouseSprite != NULL)
    {
        msspr = lbDisplay.MouseSprite;
        LbMouseGetSpriteOffset(&hot_x, &hot_y);
    }
    LbMouseChangeSprite(NULL);

    if (lbScreenSurface != NULL)
        unlock_screen ();

    if (lbHasSecondSurface)
        SDL_UnlockSurface (to_SDLSurf(lbDrawSurface));

  // lbDisplay.OldVideoMode which is DWORD 1E2EB6 is used in
  // 000ED764 sub_ED764 to probably get back to text mode
  // I'm setting it to 0xFF for now
  if (!lbDisplay.OldVideoMode)
    lbDisplay.OldVideoMode = 0xFF;

  // We are using only lbDisplay.VesaIsSetUp = false

  /*  if(lbScreenModeInfo[mode].mode < 256)
  {
    lbDisplay.VesaIsSetUp = false;
  }
  else
  {
    // lbDisplay.VesaIsSetUp = true;
    lbDisplay.VesaIsSetUp = false; // Defaulting to false
  }*/
  lbDisplay.VesaIsSetUp = false;

  // Setting mode
    if (lbHasSecondSurface)
        SDL_FreeSurface (to_SDLSurf(lbDrawSurface));

  flags = SDL_SWSURFACE;

    mdinfo = LbScreenGetModeInfo(mode);
    if ((mdinfo->VideoMode & Lb_VF_WINDOWED) == 0)
        flags |= SDL_FULLSCREEN;


    // Stretch lowres ?
    if (width == 320 && height == 200 && display_lowres_stretch)
      mdinfo = LbScreenGetModeInfo(13);

    // Init mode
    lbScreenSurface = lbDrawSurface =
          SDL_SetVideoMode(mdinfo->Width, mdinfo->Height,
 		    mdinfo->BitsPerPixel, flags);

    mdinfo = LbScreenGetModeInfo(mode);

#ifdef DEBUG
  printf ("SDL_SetVideoMode(%ld, %ld, %d, SDL_SWSURFACE) - %s\n",
          width, height, (int)lbScreenModeInfo[mode].BitsPerPixel,
          lbScreenModeInfo[mode].Desc);
#endif

  if (lbScreenSurface == NULL)
    {
      fprintf (stderr, "SDL_SetVideoMode: %s\n", SDL_GetError ());
      goto err;
    }

    SDL_WM_SetCaption(lbDrawAreaTitle, lbDrawAreaTitle);
    LbScreenUpdateIcon();

    // Create secondary surface if necessary, that is if BPP != lbEngineBPP.
    if (width == 320 && height == 200 && display_lowres_stretch)
    {
#if 0
        lbDrawSurface = SDL_CreateRGBSurface(SDL_SWSURFACE, width, height, 8, 0, 0, 0, 0);
        if (lbDrawSurface == NULL) {
            printf("Can't create secondary surface: %s",SDL_GetError());
            LbScreenReset();
            return Lb_FAIL;
        }
        lbHasSecondSurface = true;

         SDL_LockSurface(to_SDLSurf(lbDrawSurface));
         display_stretch_buffer = to_SDLSurf(lbDrawSurface)->pixels;
#endif
      // Allocate buffer
      if (display_stretch_buffer == NULL)
        {
          display_stretch_buffer = xmalloc(320 * 240);
        }
    }
    else
    {
      // Remove buffer if any
      if (display_stretch_buffer != NULL)
        {
          xfree (display_stretch_buffer);
          display_stretch_buffer = NULL;
        }

    }

    lbDisplay.DrawFlags = 0;
    lbDisplay.DrawColour = 0;
    lbDisplay.PhysicalScreenWidth = mdinfo->Width;
    lbDisplay.PhysicalScreenHeight = mdinfo->Height;
    lbDisplay.ScreenMode = mode;

    lbDisplay.PhysicalScreen = NULL;
    //lbDisplay.WScreen = NULL;
    lbDisplay.GraphicsWindowPtr = NULL;

  // Is that better? chack in disassembly
  lbDisplay.GraphicsScreenWidth  = width;
  lbDisplay.GraphicsScreenHeight = height;

    lbScreenInitialised = true;
    printf("Mode %dx%dx%d setup succeeded", (int)to_SDLSurf(lbScreenSurface)->w, (int)to_SDLSurf(lbScreenSurface)->h,
      (int)to_SDLSurf(lbScreenSurface)->format->BitsPerPixel);

  // Setup palette
  if (palette != NULL)
    {
        if (LbPaletteSet(palette) != 1)
            goto err;
    }

  // Call funcitons that recalculate some buffers
  // They can be switched to C++ later, but it's not needed
  LbScreenSetGraphicsWindow(0, 0, lbDisplay.GraphicsScreenWidth, lbDisplay.GraphicsScreenHeight);
  LbTextSetWindow(0, 0, lbDisplay.GraphicsScreenWidth, lbDisplay.GraphicsScreenHeight);

  lbScreenInitialised = true;

    lbDisplay.WScreen = wscreen_bak;
    lock_screen ();

    // set vga buffer address
    if (display_stretch_buffer != NULL)
    {
      // Set the temporary buffer
      lbDisplay.PhysicalScreen = display_stretch_buffer;
    }
    else
    {
      // Set the good buffer
      lbDisplay.PhysicalScreen = to_SDLSurf(lbScreenSurface)->pixels;
    }

  return 1;

err:
  if (lbDrawSurface != NULL)
    {
      unlock_screen ();
      SDL_FreeSurface (to_SDLSurf(lbDrawSurface));
      lbDrawSurface = NULL;
    }

  if (display_stretch_buffer)
    {
      xfree (display_stretch_buffer);
      display_stretch_buffer = NULL;
    }

  lbScreenInitialised = false;

  return -1;
}

TbResult LbScreenSetup(TbScreenMode mode, TbScreenCoord width, TbScreenCoord height,
    unsigned char *palette)
{
    return LbScreenSetupAnyModeTweaked(mode, width, height, palette);
}

void
display_update (void)
{
  assert(lbScreenSurface != NULL);
  // Stretched lowres in action?
  if (display_stretch_buffer != NULL)
    {
      // Stretch lowres
      int i, j;
      unsigned char *poutput = (unsigned char*) to_SDLSurf(lbScreenSurface)->pixels;
      unsigned char *pinput  = display_stretch_buffer;

      for (j = 0; j < 480; j++)
        {
          for (i = 0; i < 640; i+=2)
            {
              // Do not touch this formula
              int input_xy = ((j * 200) / 480) * 320 + i / 2;
              int output_xy = j * 640 + i;

              poutput[output_xy]     = pinput[input_xy];
              poutput[output_xy + 1] = pinput[input_xy];
            }
        }
  }

  SDL_Flip (to_SDLSurf(lbScreenSurface));
}

void
display_set_full_screen (bool full_screen)
{
    TbScreenModeInfo *mdinfo;

    if (lbScreenSurface != NULL)
        return;

    if (full_screen) {
        mdinfo = LbScreenGetModeInfo(1);
        mdinfo->VideoMode &= ~Lb_VF_WINDOWED;
        mdinfo = LbScreenGetModeInfo(13);
        mdinfo->VideoMode &= ~Lb_VF_WINDOWED;
    } else {
        mdinfo = LbScreenGetModeInfo(1);
        mdinfo->VideoMode |= Lb_VF_WINDOWED;
        mdinfo = LbScreenGetModeInfo(13);
        mdinfo->VideoMode |= Lb_VF_WINDOWED;
    }
}

void
display_get_size (size_t *width, size_t *height)
{
  if (lbDisplay.PhysicalScreen == NULL)
    {
      if (width != NULL)
        *width  = 0;

      if (height != NULL)
        *height = 0;

      return;
    }

  if (width != NULL)
    *width  = lbDisplay.GraphicsScreenWidth;

  if (height != NULL)
    *height = lbDisplay.GraphicsScreenHeight;
}

void
display_get_physical_size (size_t *width, size_t *height)
{
  if (lbDisplay.PhysicalScreen == NULL || lbScreenSurface == NULL)
    {
      if (width != NULL)
        *width  = 0;

      if (height != NULL)
        *height = 0;

      return;
    }

  if (width != NULL)
    *width  = to_SDLSurf(lbScreenSurface)->w;

  if (height != NULL)
    *height = to_SDLSurf(lbScreenSurface)->h;
}

void *
display_get_buffer (void)
{
  return lbDisplay.PhysicalScreen;
}

void
display_set_lowres_stretch (bool stretch)
{
  display_lowres_stretch = stretch;
}

bool
display_is_stretching_enabled (void)
{
  return (bool) (display_stretch_buffer != NULL);
}

void
display_lock (void)
{
  lock_screen ();
}

void
display_unlock (void)
{
  unlock_screen ();
}
