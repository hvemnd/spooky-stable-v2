/* SPDX-License-Identifier: MPL-2.0
 * Spooky Stable v2: the one procedural unicorn rig (design 2.6). Every toy, every pose: one draw_unicorn(pose).
 * Back to front: shadow, tail, far legs, body, near legs, neck, mane, head, horn, face, accessory; every part has a
 * 1.5 px soft-brown outline pass under it. Squash and stretch are about the anchor at the hooves. */
#pragma once

enum { EY_OPEN, EY_BLINK, EY_SPIRAL, EY_WIDE, EY_SQUINT, EY_HAPPY, EY_SLEEP };
enum { MO_CLOSED, MO_OPEN, MO_O, MO_GRIN, MO_STRAIN };
enum { ACC_FLOWERS, ACC_NIGHTCAP, ACC_CAP, ACC_HALO, ACC_TIARA, ACC_WITCH, ACC_BOW, ACC_GOGGLES, ACC_PARTY,
       ACC_CROWN, ACC_PROPELLER, ACC_BUNNY, NACC, ACC_NONE = 255 };
#define NMANES 6

typedef struct {
    /* the design's pose */
    float x, y;                       /* the anchor: between the hooves, on the ground */
    float scale_x, scale_y;           /* squash and stretch, about the anchor */
    float rot_deg;                    /* lean: + leans back (nose up) */
    float head_rot;                   /* + tilts the head up */
    float leg_phase;                  /* a walk: radians */
    float tail_phase;                 /* 0 hanging .. 1 up high */
    float mane_phase;                 /* the mane's and tail's wobble clock (ms) */
    uint8_t eye_state, mouth_state, accessory_id;
    int8_t facing;                    /* +1 right, -1 left */
    float blush;                      /* 0..1 */
    uint16_t tint;                    /* != 0: one solid colour (the impact silhouette) */
    /* what the toys needed besides (v2's extensions) */
    float scale;                      /* overall size (1 = 130 px tall) */
    float lift;                       /* off the ground: the shadow stays and shrinks */
    float belly;                      /* 1 normal; a burp inflates it */
    float cheeks;                     /* 0..1 puffed */
    float horn_glow;                  /* 0..1 */
    float look_x, look_y;             /* where the pupils point, -1..1 */
    float spread;                     /* legs braced apart, 0..1 */
    float arm_l, arm_r;               /* front legs raised as arms: 0 down .. 1 up in front (drums, the nose, a wave) */
    float vibrate;                    /* px of buzz (a charge) */
    float pick;                       /* 0..1: the near front hoof up to the nose (Booger) */
    uint8_t mane;                     /* the wardrobe's mane colours */
} pose_t;

/* the secondary motion one unicorn carries between frames: the tail's lag, the hat's jiggle, the blinking */
typedef struct {
    float tail, tail_v;
    float acc_x, acc_y, acc_vx, acc_vy;
    float hx, hy;                     /* where the head was */
    float blink_at, blink_until;
    float lx, ly;
    uint8_t init;
} rig_t;

static float uni_hoof[2][2];          /* draw_unicorn leaves its front hooves here (Drum Jam's sticks) */

static pose_t pose_rest(float x, float y, float scale);
static void rig_update(rig_t *r, pose_t *p, float T, float dt);   /* lags, jiggle, blinks: into the pose */
static void draw_unicorn(const pose_t *p, const rig_t *r);
static void draw_accessory(int id, float x, float y, float s, float rot, float T);
static uint16_t mane_color(int pal, int i);
