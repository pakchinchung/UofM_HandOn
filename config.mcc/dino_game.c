/*
 * @file dino_game.c
 *
 * @brief Chrome-style endless runner on the SSD1306.
 */

#include <stdio.h>
#include "dino_game.h"
#include "ssd1306.h"
#include "gfx_assets.h"
#include "mcp23008.h"
#include "pot.h"
#include "millis.h"

/* Vertical layout. Everything that moves is kept inside pages 1..6 so the
 * per-frame payload is 769 bytes rather than 1025. The ground deliberately sits
 * at y = 54, the last row of page 6, leaving page 7 permanently clean. */
#define GROUND_Y      (54)
#define GAME_PAGE_TOP    (1U)
#define GAME_PAGE_BOTTOM (6U)

#define HUD_Y         (0)

#define DINO_X        (10)
#define DINO_W        (12)
#define DINO_H        (16)
#define DINO_GROUND_Y (GROUND_Y - DINO_H)

/* Positions and speeds are 1/16 pixel fixed point, so slow scrolling stays
 * smooth instead of stepping a whole pixel at a time. */
#define FP_SHIFT (4)
#define FP_ONE   (1 << FP_SHIFT)

/* Gravity per tick, in 1/16 pixel per tick per tick. */
#define GRAVITY (4)

/* How high the jump should reach, in whole pixels. This is the knob to turn.
 * The impulse needed to achieve it is derived from GRAVITY at start-up by
 * GAME_JumpVelocityFor(), so the two can never drift out of step. */
#define JUMP_APEX_PX (24)

/* Highest the sprite may go: the top of the cleared play area. */
#define DINO_MIN_Y (GAME_PAGE_TOP * 8)

/* Scroll speed range the potentiometer selects, in 1/16 pixel per tick.
 * 16 is 1.0 px/tick (50 px/s), 52 is 3.25 px/tick (163 px/s). Tuned down for a
 * young player: fully counter-clockwise is a gentle walking pace, and even fully
 * clockwise is slower than the original setting. */
#define SPEED_MIN (16)
#define SPEED_MAX (52)

/* Extra speed earned through the run. Ramps half as fast as before and caps at
 * +1 px/tick, so a long run gets gradually harder without becoming unplayable. */
#define SPEED_RAMP_DIVISOR (300U)
#define SPEED_RAMP_MAX     (16)

#define OBSTACLE_COUNT (3U)

/* Horizontal gap between obstacles, in whole pixels. Widened so obstacles arrive
 * one at a time with clear space between them, rather than in clusters. At the
 * slowest speed a 64 pixel gap is about 1.3 seconds of warning. */
#define GAP_MIN (64)
#define GAP_VAR (60)

/* Collision boxes are inset so a near miss reads as a miss. Generous on purpose:
 * clipping the edge of a cactus forgives rather than punishes. */
#define DINO_HIT_INSET_X (3)
#define DINO_HIT_INSET_Y (3)

typedef enum
{
    GAME_STATE_TITLE = 0,
    GAME_STATE_PLAY,
    GAME_STATE_OVER,
} game_state_t;

typedef struct
{
    int16_t x;              /* 1/16 pixel, left edge */
    const sprite_t *sprite;
    bool active;
} obstacle_t;

static game_state_t gameState = GAME_STATE_TITLE;

static int16_t dinoY = (int16_t)(DINO_GROUND_Y * FP_ONE);
static int16_t dinoVelocity = 0;
static bool dinoAirborne = false;

/* Derived from GRAVITY and JUMP_APEX_PX in GAME_Initialize(). */
static int16_t jumpVelocity = 0;

static obstacle_t obstacles[OBSTACLE_COUNT];
static int16_t scrollSpeed = SPEED_MIN;
static uint16_t nextGap = GAP_MIN;

static uint16_t gameScore = 0;
static uint16_t gameHighScore = 0;
static uint32_t distanceAccumulator = 0;

static uint8_t runFrame = 0;
static uint8_t runFrameTimer = 0;

/* Ground texture scroll offset, kept inside page 6 so it costs no extra pages. */
static uint8_t groundOffset = 0;

static uint32_t lastTick = 0;
static uint32_t lastFrame = 0;

/* HUD is only redrawn when a displayed number changes, which keeps page 0 out
 * of the dirty set on almost every frame. */
static uint16_t hudScoreShown = 0xFFFFU;
static uint16_t hudHighShown = 0xFFFFU;
static bool hudForceRedraw = true;

/* 16-bit Galois LFSR. Seeded from millis() on the first button press so the
 * obstacle pattern differs between runs; without a seed every power-up would
 * produce an identical course. */
static uint16_t rngState = 0xACE1U;
static bool rngSeeded = false;

static uint16_t GAME_Random(void)
{
    uint16_t lsb = (uint16_t)(rngState & 1U);

    rngState >>= 1;

    if (0U != lsb)
    {
        rngState ^= 0xB400U;
    }

    return rngState;
}

static void GAME_RandomSeed(void)
{
    if (!rngSeeded)
    {
        uint16_t seed = (uint16_t)millis();

        /* Never seed an LFSR with zero; it would lock at zero forever. */
        rngState = (0U != seed) ? seed : 0xACE1U;
        rngSeeded = true;
    }
}

/* Integer square root by Newton's method. No floating point, and no libm. */
static uint16_t GAME_Sqrt(uint32_t value)
{
    uint32_t guess;
    uint32_t next;

    if (value < 2UL)
    {
        return (uint16_t)value;
    }

    guess = value;
    next = (guess + (value / guess)) / 2UL;

    while (next < guess)
    {
        guess = next;
        next = (guess + (value / guess)) / 2UL;
    }

    return (uint16_t)guess;
}

/**
 * @brief Actual rise of a jump, in subpixels, for an impulse against a gravity.
 *
 * The sprite moves by the whole velocity once per tick, so the rise is the sum
 * of the velocity at each tick until it turns positive. Over n ticks that is
 * n*v - g*n*(n-1)/2, which exceeds the continuous v^2/(2g) by roughly v/2.
 */
static uint16_t GAME_JumpRiseGet(uint16_t impulse, uint16_t gravity)
{
    uint16_t ticks = (uint16_t)((impulse + gravity - 1U) / gravity);

    return (uint16_t)(((uint32_t)ticks * impulse) -
                      (((uint32_t)gravity * ticks * (ticks - 1U)) / 2UL));
}

/**
 * @brief Impulse required to reach a given apex against a given gravity.
 *
 * Inverts the discrete rise above. Setting n*v - g*n*(n-1)/2 = h and
 * substituting n = v/g gives v^2 + g*v - 2*g*h = 0, so
 *
 *     v = (-g + sqrt(g^2 + 8*g*h)) / 2
 *
 * with h in subpixels. The closed form is approximate because n is an integer,
 * so the result is then trimmed against the exact rise until it fits the
 * headroom between the ground and the top of the cleared play area. Overshooting
 * that would push the sprite into page 0, which is not cleared every frame.
 *
 * @param apexPixels - Desired jump height in whole pixels.
 * @param gravity - Downward acceleration in subpixels per tick per tick.
 * @return Negative impulse to load into the vertical velocity.
 */
static int16_t GAME_JumpVelocityFor(uint16_t apexPixels, uint16_t gravity)
{
    uint32_t apex = (uint32_t)apexPixels * FP_ONE;
    uint16_t headroom = (uint16_t)((DINO_GROUND_Y - DINO_MIN_Y) * FP_ONE);
    uint32_t discriminant;
    uint16_t impulse;

    if (0U == gravity)
    {
        return 0;
    }

    discriminant = ((uint32_t)gravity * gravity) + (8UL * gravity * apex);
    impulse = (uint16_t)((GAME_Sqrt(discriminant) - gravity) / 2U);

    /* Never let the derived arc exceed the drawable area. */
    while ((impulse > gravity) && (GAME_JumpRiseGet(impulse, gravity) > headroom))
    {
        impulse--;
    }

    return -(int16_t)impulse;
}

static void GAME_ObstaclesClear(void)
{
    uint8_t i;

    for (i = 0; i < OBSTACLE_COUNT; i++)
    {
        obstacles[i].active = false;
        obstacles[i].x = 0;
        obstacles[i].sprite = NULL;
    }
}

static void GAME_Reset(void)
{
    dinoY = (int16_t)(DINO_GROUND_Y * FP_ONE);
    dinoVelocity = 0;
    dinoAirborne = false;

    GAME_ObstaclesClear();

    gameScore = 0;
    distanceAccumulator = 0;
    nextGap = GAP_MIN;
    runFrame = 0;
    runFrameTimer = 0;
    groundOffset = 0;

    hudForceRedraw = true;
}

/* Rightmost edge of the furthest-right active obstacle, in whole pixels.
 * Returns a negative value when the field is empty. */
static int16_t GAME_RightmostObstacleEdge(void)
{
    int16_t rightmost = -1;
    uint8_t i;

    for (i = 0; i < OBSTACLE_COUNT; i++)
    {
        if (obstacles[i].active)
        {
            int16_t edge = (int16_t)(obstacles[i].x >> FP_SHIFT) +
                           (int16_t)obstacles[i].sprite->width;

            if (edge > rightmost)
            {
                rightmost = edge;
            }
        }
    }

    return rightmost;
}

static void GAME_ObstacleSpawn(void)
{
    uint8_t i;

    for (i = 0; i < OBSTACLE_COUNT; i++)
    {
        if (!obstacles[i].active)
        {
            uint16_t roll = GAME_Random();

            obstacles[i].active = true;
            obstacles[i].x = (int16_t)(SSD1306_WIDTH * FP_ONE);

            /* Weighted towards the short cactus: tiny 4 in 8, small 3 in 8,
             * large 1 in 8. The big one stays in as an occasional challenge
             * rather than a regular wall. */
            {
                uint8_t pick = (uint8_t)(roll & 0x7U);

                if (pick < 4U)
                {
                    obstacles[i].sprite = &spriteCactusTiny;
                }
                else if (pick < 7U)
                {
                    obstacles[i].sprite = &spriteCactusSmall;
                }
                else
                {
                    obstacles[i].sprite = &spriteCactusLarge;
                }
            }

            nextGap = (uint16_t)(GAP_MIN + ((roll >> 3) % GAP_VAR));
            break;
        }
    }
}

/* Axis-aligned overlap test between the dino and one obstacle. */
static bool GAME_HasCollided(const obstacle_t *obstacle)
{
    int16_t dinoTop = (int16_t)(dinoY >> FP_SHIFT);
    int16_t dinoLeft = DINO_X + DINO_HIT_INSET_X;
    int16_t dinoRight = (DINO_X + DINO_W) - DINO_HIT_INSET_X;
    int16_t dinoBottom = dinoTop + DINO_H;

    int16_t obsLeft = (int16_t)(obstacle->x >> FP_SHIFT) + 1;
    int16_t obsRight = ((int16_t)(obstacle->x >> FP_SHIFT) +
                        (int16_t)obstacle->sprite->width) - 1;
    int16_t obsTop = (GROUND_Y - (int16_t)obstacle->sprite->height) + 1;

    dinoTop += DINO_HIT_INSET_Y;

    return ((dinoRight > obsLeft) && (dinoLeft < obsRight) &&
            (dinoBottom > obsTop) && (dinoTop < GROUND_Y));
}

/* Button edges arrive from the shell rather than being read here, so GP7 can be
 * handled globally as "return to the menu" without stealing the press from us. */
static void GAME_Step(uint8_t pressed)
{
    uint8_t i;

    if (GAME_STATE_PLAY != gameState)
    {
        if (0U != (pressed & BTN_START))
        {
            GAME_RandomSeed();
            GAME_Reset();
            gameState = GAME_STATE_PLAY;
        }

        return;
    }

    /* Base speed comes from the potentiometer, then ramps with distance. */
    {
        int16_t ramp = (int16_t)(gameScore / SPEED_RAMP_DIVISOR);

        if (ramp > SPEED_RAMP_MAX)
        {
            ramp = SPEED_RAMP_MAX;
        }

        scrollSpeed = (int16_t)POT_ScaledGet(SPEED_MIN, SPEED_MAX) + ramp;
    }

    /* Jump. Only from the ground, so holding the button cannot fly. */
    if ((0U != (pressed & BTN_JUMP)) && !dinoAirborne)
    {
        dinoVelocity = jumpVelocity;
        dinoAirborne = true;
    }

    if (dinoAirborne)
    {
        dinoVelocity += GRAVITY;
        dinoY += dinoVelocity;

        /* Hard ceiling at the top of the cleared region. Without this, retuning
         * the jump could push the sprite into page 0 and smear the score row,
         * since that page is only redrawn when the number changes. */
        if (dinoY < (int16_t)(DINO_MIN_Y * FP_ONE))
        {
            dinoY = (int16_t)(DINO_MIN_Y * FP_ONE);

            if (dinoVelocity < 0)
            {
                dinoVelocity = 0;
            }
        }

        if (dinoY >= (int16_t)(DINO_GROUND_Y * FP_ONE))
        {
            dinoY = (int16_t)(DINO_GROUND_Y * FP_ONE);
            dinoVelocity = 0;
            dinoAirborne = false;
        }
    }
    else
    {
        /* Leg animation only while running. */
        runFrameTimer++;
        if (runFrameTimer >= 4U)
        {
            runFrameTimer = 0;
            runFrame ^= 1U;
        }
    }

    /* Scroll obstacles and retire the ones that leave to the left. */
    for (i = 0; i < OBSTACLE_COUNT; i++)
    {
        if (obstacles[i].active)
        {
            obstacles[i].x -= scrollSpeed;

            if (((int16_t)(obstacles[i].x >> FP_SHIFT) +
                 (int16_t)obstacles[i].sprite->width) < 0)
            {
                obstacles[i].active = false;
            }
            else if (GAME_HasCollided(&obstacles[i]))
            {
                if (gameScore > gameHighScore)
                {
                    gameHighScore = gameScore;
                }

                gameState = GAME_STATE_OVER;
                hudForceRedraw = true;
                printf("Game over. Score %u, best %u\r\n", gameScore, gameHighScore);
                return;
            }
            else
            {
                /* Still in play. */
            }
        }
    }

    {
        int16_t rightmost = GAME_RightmostObstacleEdge();

        if (rightmost < ((int16_t)SSD1306_WIDTH - (int16_t)nextGap))
        {
            GAME_ObstacleSpawn();
        }
    }

    /* Score tracks distance travelled. */
    distanceAccumulator += (uint32_t)scrollSpeed;
    gameScore = (uint16_t)(distanceAccumulator / (FP_ONE * 4UL));

    groundOffset = (uint8_t)((groundOffset + (uint8_t)(scrollSpeed >> FP_SHIFT)) % 8U);
}

static void GAME_HudDraw(void)
{
    if (hudForceRedraw || (gameScore != hudScoreShown) || (gameHighScore != hudHighShown))
    {
        int16_t cursor;

        SSD1306_PagesClear(0U, 0U);

        cursor = SSD1306_TextDraw(0, HUD_Y, "HI");
        cursor = SSD1306_TextDrawUint(cursor + 3, HUD_Y, gameHighScore, 5U);
        (void)SSD1306_TextDrawUint(SSD1306_WIDTH - (5 * FONT_ADVANCE), HUD_Y,
                                   gameScore, 5U);

        hudScoreShown = gameScore;
        hudHighShown = gameHighScore;
        hudForceRedraw = false;
    }
}

static void GAME_GroundDraw(void)
{
    uint8_t x;

    SSD1306_HLineDraw(0, GROUND_Y, SSD1306_WIDTH, SSD1306_PIXEL_SET);

    /* Scrolling speckles just above the line, to sell the motion. Kept at
     * y = 51..52 so they stay inside page 6 alongside the ground line. */
    for (x = 0; x < SSD1306_WIDTH; x += 8U)
    {
        int16_t px = (int16_t)x + (int16_t)((8U - groundOffset) % 8U);

        SSD1306_PixelDraw(px, GROUND_Y - 3, SSD1306_PIXEL_SET);
        SSD1306_PixelDraw(px + 4, GROUND_Y - 2, SSD1306_PIXEL_SET);
    }
}

static void GAME_Render(void)
{
    const sprite_t *dinoSprite;
    int16_t dinoTop = (int16_t)(dinoY >> FP_SHIFT);
    uint8_t i;

    GAME_HudDraw();

    /* Single clear of the play area. Pages 1..6 cover the jump apex down to the
     * ground line; page 7 is never touched. */
    SSD1306_PagesClear(GAME_PAGE_TOP, GAME_PAGE_BOTTOM);

    switch (gameState)
    {
        case GAME_STATE_TITLE:
            SSD1306_TextDrawCentred(16, "DINO RUN");
            SSD1306_TextDrawCentred(30, "GP6 START  GP5 JUMP");
            SSD1306_TextDrawCentred(40, "POT SETS SPEED");
            GAME_GroundDraw();
            SSD1306_SpriteDraw(DINO_X, DINO_GROUND_Y, &spriteDinoStand);
            break;

        case GAME_STATE_OVER:
            SSD1306_TextDrawCentred(14, "GAME OVER");
            SSD1306_TextDrawCentred(26, "GP6 RETRY  GP7 MENU");
            GAME_GroundDraw();
            SSD1306_SpriteDraw(DINO_X, DINO_GROUND_Y, &spriteDinoStand);

            /* Leave the obstacle that ended the run on screen. */
            for (i = 0; i < OBSTACLE_COUNT; i++)
            {
                if (obstacles[i].active)
                {
                    SSD1306_SpriteDraw((int16_t)(obstacles[i].x >> FP_SHIFT),
                                       GROUND_Y - (int16_t)obstacles[i].sprite->height,
                                       obstacles[i].sprite);
                }
            }
            break;

        case GAME_STATE_PLAY:
        default:
            GAME_GroundDraw();

            if (dinoAirborne)
            {
                dinoSprite = &spriteDinoStand;
            }
            else
            {
                dinoSprite = (0U != runFrame) ? &spriteDinoRunB : &spriteDinoRunA;
            }

            SSD1306_SpriteDraw(DINO_X, dinoTop, dinoSprite);

            for (i = 0; i < OBSTACLE_COUNT; i++)
            {
                if (obstacles[i].active)
                {
                    SSD1306_SpriteDraw((int16_t)(obstacles[i].x >> FP_SHIFT),
                                       GROUND_Y - (int16_t)obstacles[i].sprite->height,
                                       obstacles[i].sprite);
                }
            }
            break;
    }

    (void)SSD1306_Update();
}

void GAME_Initialize(void)
{
    SSD1306_Clear();
    (void)SSD1306_Update();

    gameState = GAME_STATE_TITLE;
    gameHighScore = 0;

    jumpVelocity = GAME_JumpVelocityFor(JUMP_APEX_PX, GRAVITY);

    GAME_Reset();

    lastTick = millis();
    lastFrame = lastTick;

    GAME_Render();

    printf("Dino ready. GP6 start, GP5 jump, GP7 menu. Pot sets speed.\r\n");
    printf("Jump: gravity %u, apex target %u px -> impulse %d, "
           "actual rise %u px over %u ticks (%u ms airtime)\r\n",
           (unsigned)GRAVITY, (unsigned)JUMP_APEX_PX, jumpVelocity,
           (unsigned)(GAME_JumpRiseGet((uint16_t)(-jumpVelocity), GRAVITY) / FP_ONE),
           (unsigned)(((uint16_t)(-jumpVelocity) + GRAVITY - 1U) / GRAVITY),
           (unsigned)((((uint16_t)(-jumpVelocity) + GRAVITY - 1U) / GRAVITY) *
                      2U * GAME_TICK_MS));
}

void GAME_Tasks(uint8_t *edges)
{
    uint32_t now = millis();

    /* Fixed timestep, so the jump arc is identical whether a frame took 18 ms or
     * 40 ms. Capped at four catch-up ticks so a long stall cannot spiral. */
    {
        uint8_t guard = 0;

        while (((now - lastTick) >= GAME_TICK_MS) && (guard < 4U))
        {
            /* Edges are accumulated by the shell between ticks, so a press made
             * mid-frame is still seen even though the logic runs at 50 Hz. */
            GAME_Step((NULL != edges) ? *edges : 0U);

            if (NULL != edges)
            {
                *edges = 0U;
            }

            lastTick += GAME_TICK_MS;
            guard++;
        }

        /* If the loop fell too far behind, give up on the backlog rather than
         * running the physics at a different rate than the player sees. */
        if ((now - lastTick) >= (GAME_TICK_MS * 4U))
        {
            lastTick = now;
        }
    }

    if ((now - lastFrame) >= GAME_FRAME_MS)
    {
        lastFrame = now;
        GAME_Render();
    }
}

uint16_t GAME_ScoreGet(void)
{
    return gameScore;
}

uint16_t GAME_HighScoreGet(void)
{
    return gameHighScore;
}
