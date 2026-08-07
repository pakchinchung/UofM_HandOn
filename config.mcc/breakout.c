/*
 * @file breakout.c
 *
 * @brief Breakout on the SSD1306, with the PD7 potentiometer as the paddle.
 */

#include <stdio.h>
#include "breakout.h"
#include "ssd1306.h"
#include "mcp23008.h"
#include "pot.h"
#include "millis.h"

/* Subpixel fixed point, as in the dino game. */
#define FP_SHIFT (4)
#define FP_ONE   (1 << FP_SHIFT)

/* Playfield. The wall sits on y = 8 so the ball never reaches page 0, which
 * holds the HUD and is only redrawn when a number changes. */
#define WALL_Y      (8)
#define FIELD_TOP   (WALL_Y + 1)
#define FIELD_BOTTOM (62)

#define HUD_Y (0)

/* Bricks: 8 columns on a 16 pixel pitch, 3 rows on a 5 pixel pitch starting at
 * y = 10, so the whole field of bricks lives inside pages 1 and 2. */
#define BRICK_COLS   (8U)
#define BRICK_ROWS   (3U)
#define BRICK_W      (15U)
#define BRICK_H      (4U)
#define BRICK_PITCH_X (16)
#define BRICK_PITCH_Y (5)
#define BRICK_ORIGIN_Y (10)

#define BRICK_SCORE (10U)

/* Paddle lives alone on page 7, which makes erase-in-place trivially safe. */
#define PADDLE_W (24)
#define PADDLE_H (3)
#define PADDLE_Y (58)

#define BALL_SIZE (3)

/* Vertical speed in subpixels per tick. 20 is 1.25 px/tick, about 63 px/s at a
 * 20 ms tick, which is gentle enough to track by eye. */
#define BALL_SPEED_BASE (20)
#define BALL_SPEED_STEP (3)
#define BALL_SPEED_MAX  (38)

/* Horizontal deflection imparted by the paddle, in subpixels per tick per pixel
 * of offset from the paddle centre. Hitting the edge sends the ball out wide. */
#define PADDLE_ENGLISH (2)
#define BALL_DX_MAX    (28)

#define LIVES_START (3U)

typedef enum
{
    BRK_STATE_TITLE = 0,
    BRK_STATE_SERVE,   /* ball parked on the paddle, waiting for GP5 */
    BRK_STATE_PLAY,
    BRK_STATE_OVER,
} brk_state_t;

static brk_state_t brkState = BRK_STATE_TITLE;

/* One bit per column, one byte per row. Bit set means the brick is intact. */
static uint8_t brickAlive[BRICK_ROWS];
static uint8_t bricksRemaining = 0;

static int16_t ballX = 0;    /* subpixels */
static int16_t ballY = 0;
static int16_t ballDx = 0;
static int16_t ballDy = 0;
static int16_t ballSpeed = BALL_SPEED_BASE;

static int16_t paddleX = 0;      /* whole pixels */
static int16_t paddleDrawnX = 0;
static int16_t ballDrawnX = 0;   /* whole pixels, last painted position */
static int16_t ballDrawnY = 0;
static bool spritesPainted = false;

static uint16_t brkScore = 0;
static uint16_t brkHighScore = 0;
static uint8_t brkLives = LIVES_START;
static uint8_t brkLevel = 1U;

static uint32_t lastTick = 0;
static uint32_t lastFrame = 0;

static uint16_t hudScoreShown = 0xFFFFU;
static uint16_t hudHighShown = 0xFFFFU;
static uint8_t hudLivesShown = 0xFFU;
static bool hudForceRedraw = true;

/* Shared LFSR pattern, seeded on first launch so the serve angle varies. */
static uint16_t rngState = 0x1234U;
static bool rngSeeded = false;

static uint16_t BRK_Random(void)
{
    uint16_t lsb = (uint16_t)(rngState & 1U);

    rngState >>= 1;

    if (0U != lsb)
    {
        rngState ^= 0xB400U;
    }

    return rngState;
}

static int16_t BRK_BrickX(uint8_t col)
{
    return (int16_t)((int16_t)col * BRICK_PITCH_X);
}

static int16_t BRK_BrickY(uint8_t row)
{
    return (int16_t)(BRICK_ORIGIN_Y + ((int16_t)row * BRICK_PITCH_Y));
}

static void BRK_BricksFill(void)
{
    uint8_t row;

    for (row = 0; row < BRICK_ROWS; row++)
    {
        brickAlive[row] = 0xFFU; /* all 8 columns */
    }

    bricksRemaining = (uint8_t)(BRICK_ROWS * BRICK_COLS);
}

static void BRK_BrickDraw(uint8_t row, uint8_t col, ssd1306_ink_t ink)
{
    SSD1306_RectFill(BRK_BrickX(col), BRK_BrickY(row), BRICK_W, BRICK_H, ink);
}

/* Repaints any surviving brick that overlaps the given rectangle.
 *
 * Erasing the ball in place is a filled clear, and the ball spends much of its
 * time touching a brick it just bounced off. Without this the clear would eat a
 * bite out of the neighbouring brick and leave it visibly notched. */
static void BRK_BricksRedrawIn(int16_t x, int16_t y, uint8_t w, uint8_t h)
{
    uint8_t row;

    for (row = 0; row < BRICK_ROWS; row++)
    {
        int16_t brickTop = BRK_BrickY(row);
        int16_t brickBottom = brickTop + (int16_t)BRICK_H;

        if ((y < brickBottom) && ((y + (int16_t)h) > brickTop))
        {
            uint8_t col;

            for (col = 0; col < BRICK_COLS; col++)
            {
                if (0U != (brickAlive[row] & (uint8_t)(1U << col)))
                {
                    int16_t brickLeft = BRK_BrickX(col);
                    int16_t brickRight = brickLeft + (int16_t)BRICK_W;

                    if ((x < brickRight) && ((x + (int16_t)w) > brickLeft))
                    {
                        BRK_BrickDraw(row, col, SSD1306_PIXEL_SET);
                    }
                }
            }
        }
    }
}

static void BRK_BallPark(void)
{
    /* Sit the ball on top of the paddle, centred. */
    ballX = (int16_t)((paddleX + ((PADDLE_W - BALL_SIZE) / 2)) * FP_ONE);
    ballY = (int16_t)((PADDLE_Y - BALL_SIZE) * FP_ONE);
    ballDx = 0;
    ballDy = 0;
}

static void BRK_LevelStart(bool resetScore)
{
    BRK_BricksFill();

    if (resetScore)
    {
        brkScore = 0;
        brkLives = LIVES_START;
        brkLevel = 1U;
        ballSpeed = BALL_SPEED_BASE;
    }

    BRK_BallPark();
    spritesPainted = false;
    hudForceRedraw = true;
    brkState = BRK_STATE_SERVE;
}

static void BRK_HudDraw(void)
{
    if (hudForceRedraw || (brkScore != hudScoreShown) ||
        (brkHighScore != hudHighShown) || (brkLives != hudLivesShown))
    {
        int16_t cursor;

        SSD1306_PagesClear(0U, 0U);

        cursor = SSD1306_TextDraw(0, HUD_Y, "HI");
        (void)SSD1306_TextDrawUint(cursor + 3, HUD_Y, brkHighScore, 5U);

        cursor = SSD1306_TextDraw(56, HUD_Y, "L");
        (void)SSD1306_TextDrawUint(cursor, HUD_Y, brkLives, 1U);

        (void)SSD1306_TextDrawUint(SSD1306_WIDTH - (5 * FONT_ADVANCE), HUD_Y,
                                   brkScore, 5U);

        hudScoreShown = brkScore;
        hudHighShown = brkHighScore;
        hudLivesShown = brkLives;
        hudForceRedraw = false;
    }
}

/* Paints the static scene: wall and every surviving brick. Called on entry to a
 * state, not per frame. */
static void BRK_SceneDraw(void)
{
    uint8_t row;
    uint8_t col;

    SSD1306_PagesClear(1U, 7U);

    SSD1306_HLineDraw(0, WALL_Y, SSD1306_WIDTH, SSD1306_PIXEL_SET);

    for (row = 0; row < BRICK_ROWS; row++)
    {
        for (col = 0; col < BRICK_COLS; col++)
        {
            if (0U != (brickAlive[row] & (uint8_t)(1U << col)))
            {
                BRK_BrickDraw(row, col, SSD1306_PIXEL_SET);
            }
        }
    }

    spritesPainted = false;
}

/* Erases the previously painted ball and paddle, repairing bricks behind the
 * ball, then paints them at their current positions. Only the pages these
 * rectangles touch end up dirty, which is the whole point of doing it this way
 * instead of clearing the play area. */
static void BRK_SpritesRepaint(void)
{
    int16_t newBallX = (int16_t)(ballX >> FP_SHIFT);
    int16_t newBallY = (int16_t)(ballY >> FP_SHIFT);

    if (spritesPainted)
    {
        if ((newBallX != ballDrawnX) || (newBallY != ballDrawnY))
        {
            SSD1306_RectFill(ballDrawnX, ballDrawnY, BALL_SIZE, BALL_SIZE,
                             SSD1306_PIXEL_CLEAR);
            BRK_BricksRedrawIn(ballDrawnX, ballDrawnY, BALL_SIZE, BALL_SIZE);
        }

        if (paddleX != paddleDrawnX)
        {
            SSD1306_RectFill(paddleDrawnX, PADDLE_Y, PADDLE_W, PADDLE_H,
                             SSD1306_PIXEL_CLEAR);
        }
    }

    SSD1306_RectFill(paddleX, PADDLE_Y, PADDLE_W, PADDLE_H, SSD1306_PIXEL_SET);
    SSD1306_RectFill(newBallX, newBallY, BALL_SIZE, BALL_SIZE, SSD1306_PIXEL_SET);

    ballDrawnX = newBallX;
    ballDrawnY = newBallY;
    paddleDrawnX = paddleX;
    spritesPainted = true;
}

/* Destroys the first intact brick the ball rectangle overlaps.
 * Returns true if something was hit. */
static bool BRK_BrickCollide(int16_t bx, int16_t by)
{
    uint8_t row;

    for (row = 0; row < BRICK_ROWS; row++)
    {
        int16_t brickTop = BRK_BrickY(row);
        int16_t brickBottom = brickTop + (int16_t)BRICK_H;

        if ((by < brickBottom) && ((by + BALL_SIZE) > brickTop))
        {
            uint8_t col;

            for (col = 0; col < BRICK_COLS; col++)
            {
                if (0U != (brickAlive[row] & (uint8_t)(1U << col)))
                {
                    int16_t brickLeft = BRK_BrickX(col);
                    int16_t brickRight = brickLeft + (int16_t)BRICK_W;

                    if ((bx < brickRight) && ((bx + BALL_SIZE) > brickLeft))
                    {
                        brickAlive[row] &= (uint8_t)~(1U << col);
                        bricksRemaining--;
                        BRK_BrickDraw(row, col, SSD1306_PIXEL_CLEAR);
                        brkScore += BRICK_SCORE;
                        return true;
                    }
                }
            }
        }
    }

    return false;
}

static void BRK_Step(uint8_t pressed)
{
    /* The paddle follows the pot in every state, so it feels live even on the
     * title screen. */
    paddleX = (int16_t)POT_ScaledGet(0U, (uint16_t)(SSD1306_WIDTH - PADDLE_W));

    switch (brkState)
    {
        case BRK_STATE_TITLE:
        case BRK_STATE_OVER:
            if (0U != (pressed & BTN_START))
            {
                if (!rngSeeded)
                {
                    uint16_t seed = (uint16_t)millis();

                    rngState = (0U != seed) ? seed : 0x1234U;
                    rngSeeded = true;
                }

                BRK_LevelStart(true);
                BRK_SceneDraw();
            }
            break;

        case BRK_STATE_SERVE:
            BRK_BallPark();

            if (0U != (pressed & BTN_JUMP))
            {
                /* Launch upward with a small random lean so repeated serves do
                 * not trace the same path. */
                ballDy = (int16_t)(-ballSpeed);
                ballDx = (int16_t)((BRK_Random() & 0x1U) ? 8 : -8);
                brkState = BRK_STATE_PLAY;
            }
            break;

        case BRK_STATE_PLAY:
        default:
            ballX += ballDx;
            ballY += ballDy;

            /* Side walls. */
            if (ballX < 0)
            {
                ballX = 0;
                ballDx = (int16_t)(-ballDx);
            }
            else if (ballX > (int16_t)((SSD1306_WIDTH - BALL_SIZE) * FP_ONE))
            {
                ballX = (int16_t)((SSD1306_WIDTH - BALL_SIZE) * FP_ONE);
                ballDx = (int16_t)(-ballDx);
            }
            else
            {
                /* Still inside the field. */
            }

            /* Ceiling. */
            if (ballY < (int16_t)(FIELD_TOP * FP_ONE))
            {
                ballY = (int16_t)(FIELD_TOP * FP_ONE);
                ballDy = (int16_t)(-ballDy);
            }

            /* Bricks. Reverse vertically, which is the readable behaviour even
             * when the ball technically arrived from the side. */
            if (BRK_BrickCollide((int16_t)(ballX >> FP_SHIFT),
                                 (int16_t)(ballY >> FP_SHIFT)))
            {
                ballDy = (int16_t)(-ballDy);

                if (0U == bricksRemaining)
                {
                    brkLevel++;

                    if (ballSpeed < BALL_SPEED_MAX)
                    {
                        ballSpeed = (int16_t)(ballSpeed + BALL_SPEED_STEP);
                    }

                    BRK_LevelStart(false);
                    BRK_SceneDraw();
                    printf("Level %u. Score %u\r\n", brkLevel, brkScore);
                    return;
                }
            }

            /* Paddle. Only counts while descending, so a ball clipping the
             * paddle from below cannot get trapped. */
            {
                int16_t bx = (int16_t)(ballX >> FP_SHIFT);
                int16_t by = (int16_t)(ballY >> FP_SHIFT);

                if ((ballDy > 0) && ((by + BALL_SIZE) >= PADDLE_Y) &&
                    (by < (PADDLE_Y + PADDLE_H)) &&
                    ((bx + BALL_SIZE) > paddleX) && (bx < (paddleX + PADDLE_W)))
                {
                    int16_t offset = (bx + (BALL_SIZE / 2)) -
                                     (paddleX + (PADDLE_W / 2));

                    ballY = (int16_t)((PADDLE_Y - BALL_SIZE) * FP_ONE);
                    ballDy = (int16_t)(-ballSpeed);
                    ballDx = (int16_t)(offset * PADDLE_ENGLISH);

                    if (ballDx > BALL_DX_MAX)
                    {
                        ballDx = BALL_DX_MAX;
                    }
                    else if (ballDx < -BALL_DX_MAX)
                    {
                        ballDx = -BALL_DX_MAX;
                    }
                    else
                    {
                        /* Within range. */
                    }
                }
            }

            /* Missed. */
            if ((int16_t)(ballY >> FP_SHIFT) > FIELD_BOTTOM)
            {
                if (brkLives > 0U)
                {
                    brkLives--;
                }

                if (0U == brkLives)
                {
                    if (brkScore > brkHighScore)
                    {
                        brkHighScore = brkScore;
                    }

                    brkState = BRK_STATE_OVER;
                    hudForceRedraw = true;
                    printf("Breakout over. Score %u, best %u\r\n",
                           brkScore, brkHighScore);
                }
                else
                {
                    BRK_BallPark();
                    brkState = BRK_STATE_SERVE;
                    /* Repaint so the ball's last position above the paddle is
                     * not left behind. */
                    BRK_SceneDraw();
                }
            }
            break;
    }
}

static void BRK_Render(void)
{
    BRK_HudDraw();

    switch (brkState)
    {
        case BRK_STATE_TITLE:
            SSD1306_PagesClear(1U, 7U);
            SSD1306_TextDrawCentred(18, "BREAKOUT");
            SSD1306_TextDrawCentred(32, "POT MOVES PADDLE");
            SSD1306_TextDrawCentred(42, "GP6 START");
            SSD1306_RectFill(paddleX, PADDLE_Y, PADDLE_W, PADDLE_H,
                             SSD1306_PIXEL_SET);
            paddleDrawnX = paddleX;
            spritesPainted = false;
            break;

        case BRK_STATE_OVER:
            SSD1306_PagesClear(1U, 7U);
            SSD1306_TextDrawCentred(18, "GAME OVER");
            (void)SSD1306_TextDrawUint(46, 30, brkScore, 5U);
            SSD1306_TextDrawCentred(42, "GP6 RETRY  GP7 MENU");
            spritesPainted = false;
            break;

        case BRK_STATE_SERVE:
        case BRK_STATE_PLAY:
        default:
            BRK_SpritesRepaint();
            break;
    }

    (void)SSD1306_Update();
}

void BRK_Initialize(void)
{
    SSD1306_Clear();
    (void)SSD1306_Update();

    brkState = BRK_STATE_TITLE;
    brkHighScore = 0;
    brkScore = 0;
    brkLives = LIVES_START;
    brkLevel = 1U;
    ballSpeed = BALL_SPEED_BASE;
    BRK_BricksFill();
    spritesPainted = false;
    hudForceRedraw = true;

    paddleX = (int16_t)POT_ScaledGet(0U, (uint16_t)(SSD1306_WIDTH - PADDLE_W));
    BRK_BallPark();

    lastTick = millis();
    lastFrame = lastTick;

    BRK_Render();

    printf("Breakout ready. Pot = paddle, GP5 launch, GP6 start, GP7 menu.\r\n");
}

void BRK_Tasks(uint8_t *edges)
{
    uint32_t now = millis();
    uint8_t guard = 0;

    while (((now - lastTick) >= BRK_TICK_MS) && (guard < 4U))
    {
        BRK_Step((NULL != edges) ? *edges : 0U);

        if (NULL != edges)
        {
            *edges = 0U;
        }

        lastTick += BRK_TICK_MS;
        guard++;
    }

    if ((now - lastTick) >= (BRK_TICK_MS * 4U))
    {
        lastTick = now;
    }

    if ((now - lastFrame) >= BRK_FRAME_MS)
    {
        lastFrame = now;
        BRK_Render();
    }
}

uint16_t BRK_ScoreGet(void)
{
    return brkScore;
}

uint16_t BRK_HighScoreGet(void)
{
    return brkHighScore;
}
