#include <SDL3/SDL_timer.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <SDL3/SDL.h>
#include <time.h>

#define GAME_WIDTH 640
#define GAME_HEIGHT 480
#define GRID_SIZE 30
#define TILE_WIDTH (GAME_WIDTH / GRID_SIZE)
#define TILE_HEIGHT (GAME_HEIGHT / GRID_SIZE)
#define INITIAL_SPEED_MS 100
#define SPEED_INCREMENT 5
#define MAX_SPEED 30
#define INITIAL_SNAKE_CAP 8

#define COLOR_BACKGROUND 34, 34, 34, 255 // Black
#define COLOR_SNAKE_BODY 76, 175, 80, 255 // Green
#define COLOR_SNAKE_HEAD 102, 196, 102, 255 // Light green
#define COLOR_FOOD 255, 82, 82, 255 // Red

typedef struct Point {
    int x;
    int y;
} Point;

enum SnakeDirection {
    SNAKE_DIRECTION_NONE,
    SNAKE_DIRECTION_UP,
    SNAKE_DIRECTION_DOWN,
    SNAKE_DIRECTION_LEFT,
    SNAKE_DIRECTION_RIGHT
};

typedef struct Snake {
    Point* body;
    size_t head;
    size_t tail;
    size_t len;
    size_t capacity;
} Snake;

typedef struct SnakeGame {
    SDL_Window* window;
    SDL_Renderer* renderer;

    bool running;
    Uint64 last_move_time;
    Uint64 game_speed;
    unsigned score;

    Point food;
    Snake* snake;
    enum SnakeDirection curr_snake_direction;
    enum SnakeDirection next_snake_direction;
} SnakeGame;

static SnakeGame* snake_game_init(int game_width, int game_height);
static void snake_game_setup(SnakeGame* snake_game);
static void snake_game_run(SnakeGame* snake_game);
static void snake_game_handle_events(SnakeGame* snake_game);
static void snake_game_update(SnakeGame* snake_game);
static void snake_game_render(SnakeGame* snake_game);
static void snake_game_destroy(SnakeGame* snake_game);
static void snake_game_spawn_food(SnakeGame* snake_game);

static bool snake_push_head(Snake* snake, Point point);
static void snake_pop_tail(Snake* snake);
static Point snake_get_body_part(Snake* snake, size_t idx);

int main(void) {
    SnakeGame* snake_game = snake_game_init(GAME_WIDTH, GAME_HEIGHT);
    if (!snake_game) {
        fprintf(stderr, "[ERROR] Failed to create the game!\n");
        return 1;
    }

    snake_game_setup(snake_game);
    snake_game_run(snake_game);
    snake_game_destroy(snake_game);

    return 0;
}

static SnakeGame* snake_game_init(int game_width, int game_height) {
    SnakeGame* snake_game = calloc(1, sizeof(SnakeGame));
    if (!snake_game) {
        fprintf(stderr, "[ERROR] calloc for SnakeGame failed\n");
        return NULL;
    }

    if (!SDL_Init(SDL_INIT_VIDEO)) {
        fprintf(stderr, "[ERROR] SDL_Init(): %s\n", SDL_GetError());
        free(snake_game);
        return NULL;
    }

    snake_game->window = SDL_CreateWindow("Snake", game_width, game_height, 0);
    if (!snake_game->window) {
        fprintf(stderr, "[ERROR] SDL_CreateWindow(): %s\n", SDL_GetError());
        free(snake_game);
        SDL_Quit();
        return NULL;
    }

    snake_game->renderer = SDL_CreateRenderer(snake_game->window, NULL);
    if (!snake_game->renderer) {
        fprintf(stderr, "[ERROR] SDL_CreateRenderer(): %s\n", SDL_GetError());
        SDL_DestroyWindow(snake_game->window);
        free(snake_game);
        SDL_Quit();
        return NULL;
    }


    return snake_game;
}

static void snake_game_setup(SnakeGame* snake_game) {
    if (!snake_game->snake) {
        Snake* snake = malloc(sizeof(Snake));
        if (!snake) {
            fprintf(stderr, "[ERROR] malloc() for Snake\n");
            snake_game->running = false;
            return;
        }

        Point* body = malloc(sizeof(Point) * INITIAL_SNAKE_CAP);
        if (!body) {
            fprintf(stderr, "[ERROR] malloc() for Point\n");
            free(snake);
            snake_game->running = false;
            return;
        }

        snake_game->snake = snake;
        snake_game->snake->body = body;
        snake_game->snake->capacity = INITIAL_SNAKE_CAP;
    }

    snake_game->snake->len = 0;
    snake_game->snake->head = 0;
    snake_game->snake->tail = 0;

    if (!snake_push_head(snake_game->snake, (Point){GRID_SIZE / 2, GRID_SIZE / 2})) {
        snake_game->running = false;
        return;
    }

    srand(time(NULL));
    snake_game_spawn_food(snake_game);

    snake_game->running = true;
    snake_game->game_speed = INITIAL_SPEED_MS;
    snake_game->score = 0;
    snake_game->last_move_time = SDL_GetTicks();
    snake_game->curr_snake_direction = SNAKE_DIRECTION_NONE;
    snake_game->next_snake_direction = SNAKE_DIRECTION_NONE;
}

static void snake_game_run(SnakeGame* snake_game) {
    while (snake_game->running) {
        snake_game_handle_events(snake_game);
        snake_game_update(snake_game);
        snake_game_render(snake_game);
        SDL_Delay(10);
    }
}

static void snake_game_handle_events(SnakeGame* snake_game) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_EVENT_QUIT) {
            snake_game->running = false;
        } else if (e.type == SDL_EVENT_KEY_DOWN) {
            switch (e.key.key) {
                case SDLK_UP:
                    if (snake_game->curr_snake_direction != SNAKE_DIRECTION_DOWN)
                        snake_game->next_snake_direction = SNAKE_DIRECTION_UP;
                    break;
                case SDLK_DOWN:
                    if (snake_game->curr_snake_direction != SNAKE_DIRECTION_UP)
                        snake_game->next_snake_direction = SNAKE_DIRECTION_DOWN;
                    break;
                case SDLK_LEFT:
                    if (snake_game->curr_snake_direction != SNAKE_DIRECTION_RIGHT)
                        snake_game->next_snake_direction = SNAKE_DIRECTION_LEFT;
                    break;
                case SDLK_RIGHT:
                    if (snake_game->curr_snake_direction != SNAKE_DIRECTION_LEFT)
                        snake_game->next_snake_direction = SNAKE_DIRECTION_RIGHT;
                    break;
                case SDLK_R:
                    snake_game_setup(snake_game);
                    break;
                case SDLK_Q:
                    snake_game->running = false;
                    break;
            }
        }
    }
}

static void snake_game_update(SnakeGame* snake_game) {
    if (SDL_GetTicks() - snake_game->last_move_time < snake_game->game_speed) {
        return;
    }
    snake_game->last_move_time = SDL_GetTicks();

    if (snake_game->next_snake_direction != SNAKE_DIRECTION_NONE) {
        snake_game->curr_snake_direction = snake_game->next_snake_direction;
        snake_game->next_snake_direction = SNAKE_DIRECTION_NONE;
    }

    if (snake_game->curr_snake_direction == SNAKE_DIRECTION_NONE) {
        return;
    }

    Point snake_head = snake_game->snake->body[snake_game->snake->head];

    switch (snake_game->curr_snake_direction) {
        case SNAKE_DIRECTION_UP: snake_head.y = (snake_head.y - 1 + GRID_SIZE) % GRID_SIZE; break;
        case SNAKE_DIRECTION_DOWN: snake_head.y = (snake_head.y + 1) % GRID_SIZE; break;
        case SNAKE_DIRECTION_LEFT: snake_head.x = (snake_head.x - 1 + GRID_SIZE) % GRID_SIZE; break;
        case SNAKE_DIRECTION_RIGHT: snake_head.x = (snake_head.x + 1) % GRID_SIZE; break;
        case SNAKE_DIRECTION_NONE:
            fprintf(stderr, "[ERROR] Unreachable case reached in %s at line %d\n", __FILE__, __LINE__);
            exit(EXIT_FAILURE);
            break;
    }

    /* Snake collision */
    for (size_t i = 1; i < snake_game->snake->len; ++i) {
        Point body_part = snake_get_body_part(snake_game->snake, i);
        if (snake_head.x == body_part.x && snake_head.y == body_part.y) {
            snake_game_setup(snake_game);
            return;
        }
    }

    if (!snake_push_head(snake_game->snake, snake_head)) {
        snake_game->running = false;
        return;
    }

    if (snake_head.x == snake_game->food.x && snake_head.y == snake_game->food.y) {
        snake_game->score++;
        if (snake_game->game_speed > MAX_SPEED) {
            snake_game->game_speed -= SPEED_INCREMENT;
        }
        snake_game_spawn_food(snake_game);
    } else {
        snake_pop_tail(snake_game->snake);
    }
}

static void snake_game_render(SnakeGame* snake_game) {
    if (!SDL_SetRenderDrawColor(snake_game->renderer, COLOR_BACKGROUND)) {
        fprintf(stderr, "[ERROR] SDL_SetRenderDrawColor(): %s\n", SDL_GetError());
        snake_game->running = false;
        return;
    }

    if (!SDL_RenderClear(snake_game->renderer)) {
        fprintf(stderr, "[ERROR] SDL_RenderClear(): %s\n", SDL_GetError());
        snake_game->running = false;
        return;
    }

    if (!SDL_SetRenderDrawColor(snake_game->renderer, COLOR_SNAKE_BODY)) {
        fprintf(stderr, "[ERROR] SDL_SetRenderDrawColor(): %s\n", SDL_GetError());
        snake_game->running = false;
        return;
    }

    /* Draw snake body */
    for (size_t i = 0; i < snake_game->snake->len - 1; ++i) {
        Point body_part = snake_get_body_part(snake_game->snake, i);
        SDL_FRect body_rect = {.x = body_part.x * TILE_WIDTH,
                               .y = body_part.y * TILE_HEIGHT,
                               .w = TILE_WIDTH,
                               .h = TILE_HEIGHT};

        if (!SDL_RenderFillRect(snake_game->renderer, &body_rect)) {
            fprintf(stderr, "[ERROR] SDL_RenderFillRect(): %s\n", SDL_GetError());
            snake_game->running = false;
            return;
        }
    }

    Point head_segment = snake_game->snake->body[snake_game->snake->head];
    SDL_FRect head_rect = {.x = head_segment.x * TILE_WIDTH,
                           .y = head_segment.y * TILE_HEIGHT,
                           .w = TILE_WIDTH,
                           .h = TILE_HEIGHT};

    if (!SDL_SetRenderDrawColor(snake_game->renderer, COLOR_SNAKE_HEAD)) {
        fprintf(stderr, "[ERROR] SDL_SetRenderDrawColor(): %s\n", SDL_GetError());
        snake_game->running = false;
        return;
    }

    if (!SDL_RenderFillRect(snake_game->renderer, &head_rect)) {
        fprintf(stderr, "[ERROR] SDL_RenderFillRect(): %s\n", SDL_GetError());
        snake_game->running = false;
        return;
    }

    /* Draw food */
    if (!SDL_SetRenderDrawColor(snake_game->renderer, COLOR_FOOD)) {
        fprintf(stderr, "[ERROR] SDL_SetRenderDrawColor(): %s\n", SDL_GetError());
        snake_game->running = false;
        return;
    }

    SDL_FRect food_rect = {.x = snake_game->food.x * TILE_WIDTH,
                           .y = snake_game->food.y * TILE_HEIGHT,
                           .w = TILE_WIDTH,
                           .h = TILE_HEIGHT};

    if (!SDL_RenderFillRect(snake_game->renderer, &food_rect)) {
        fprintf(stderr, "[ERROR] SDL_RenderFillRect(): %s\n", SDL_GetError());
        snake_game->running = false;
        return;
    }

    char title[100];
    snprintf(title, 100, "Score: %d", snake_game->score);
    if (!SDL_SetWindowTitle(snake_game->window, title)) {
        fprintf(stderr, "[ERROR] SDL_SetWindowTitle(): %s\n", SDL_GetError());
        snake_game->running = false;
        return;
    }

    if (!SDL_RenderPresent(snake_game->renderer)) {
        fprintf(stderr, "[ERROR] SDL_RenderPresent(): %s\n", SDL_GetError());
        snake_game->running = false;
        return;
    }
}

static void snake_game_destroy(SnakeGame* snake_game) {
    if (!snake_game) return;

    SDL_DestroyRenderer(snake_game->renderer);
    SDL_DestroyWindow(snake_game->window);

    if (snake_game->snake) {
        free(snake_game->snake->body);
        free(snake_game->snake);
    }
    free(snake_game);

    SDL_Quit();
}

static void snake_game_spawn_food(SnakeGame* snake_game) {
    bool on_snake;
    do {
        on_snake = false;
        snake_game->food.x = rand() % GRID_SIZE;
        snake_game->food.y = rand() % GRID_SIZE;

        for (size_t i = 0; i < snake_game->snake->len; ++i) {
            Point body_part = snake_get_body_part(snake_game->snake, i);
            if (snake_game->food.x == body_part.x && snake_game->food.y == body_part.y) {
                on_snake = true;
                break;
            }
        }
    } while (on_snake);
}

static bool snake_push_head(Snake* snake, Point point) {
    if (snake->len + 1 > snake->capacity) {
        Point* new_snake_body = malloc(sizeof(Point) * snake->capacity * 2);
        if (!new_snake_body) {
            fprintf(stderr, "[ERROR] malloc() snake body\n");
            return false;
        }

        for (size_t i = 0; i < snake->len; ++i) {
            new_snake_body[i] = snake_get_body_part(snake, i);
        }

        free(snake->body);
        snake->body = new_snake_body;
        snake->head = snake->len - 1;
        snake->tail = 0;
        snake->capacity *= 2;
    }

    if (snake->len > 0) {
        snake->head = (snake->head + 1) % snake->capacity;
    }

    snake->body[snake->head] = point;
    snake->len++;

    return true;
}

static void snake_pop_tail(Snake* snake) {
    if (snake->len == 0) return;
    snake->tail = (snake->tail + 1) % snake->capacity;
    snake->len--;
}

static Point snake_get_body_part(Snake* snake, size_t idx) {
    size_t body_idx = (snake->tail + idx) % snake->capacity;
    return snake->body[body_idx];
}
