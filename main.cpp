#define _USE_MATH_DEFINES
#define M_RAD 57.2958 // One radian equals 57.2958 degrees

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#define SCREEN_WIDTH	1280
#define SCREEN_HEIGHT	600
#define DEFAULT_X 100
#define DEFAULT_Y 300
#define STARTING_X_VELOCITY 5.
#define GRAVITY .3
#define DRAG 10 // Max y velocity due to gravity. Named this way as it implements a simplified concept of drag balancing gravity.
#define JUMP_STRENGTH 7.5
#define JUMP_INITIAL_PUSH -3.;
#define Y_ACC_CONST -0.5
#define NUMBER_0F_LIVES 3
#define TICK_PERIOD 15 // Number of milliseconds between ticks. The smaller this number, the faster the game goes.
                        // 30 gives a fairly dynamic gameplay
                        // 200 is pretty good for slo-mo gameplay for debugging purposes.
                        // 3 is good for a decent fast-forward.

using namespace std;

// draw a text txt on surface screen, starting from the point (x, y)
// charset is a 128x128 bitmap containing character images
void DrawString(SDL_Surface *screen, int x, int y, const char *text, SDL_Surface *charset) {
	int px, py, c;
	SDL_Rect s, d;
	s.w = 8;
	s.h = 8;
	d.w = 8;
	d.h = 8;
	while(*text) {
		c = *text & 255;
		px = (c % 16) * 8;
		py = (c / 16) * 8;
		s.x = px;
		s.y = py;
		d.x = x;
		d.y = y;
		SDL_BlitSurface(charset, &s, screen, &d);
		x += 8;
		text++;
    }
}


void DrawSurface(SDL_Surface *screen, SDL_Surface *sprite, int x, int y) {
	SDL_Rect dest;
	dest.x = x - sprite->w / 2;
	dest.y = y - sprite->h / 2;
	dest.w = sprite->w;
	dest.h = sprite->h;
	SDL_BlitSurface(sprite, NULL, screen, &dest);
}


void DrawPixel(SDL_Surface *surface, double x, double y, Uint32 color) {
    if (x < SCREEN_WIDTH && x >= 0 && y < SCREEN_HEIGHT && y >= 0) {
        int bpp = surface->format->BytesPerPixel;
        Uint8 *p = (Uint8 *)surface->pixels + (int)y * surface->pitch + (int)x * bpp;
        *(Uint32 *)p = color;
	}
}


// draw a vertical (when dx = 0, dy = 1) or horizontal (when dx = 1, dy = 0) line
void DrawLine(SDL_Surface *screen, int x, int y, int l, int dx, int dy, Uint32 color) {
	for(int i = 0; i < l; i++) {
        DrawPixel(screen, x, y, color);
        x += dx;
        y += dy;
    }
}


// draw a rectangle of size l by k
void DrawRectangle(SDL_Surface *screen, int x, int y, int l, int k, Uint32 outlineColor, Uint32 fillColor) {
	DrawLine(screen, x, y, k, 0, 1, outlineColor); // Left side
	DrawLine(screen, x + l - 1, y, k, 0, 1, outlineColor); // Right side
	DrawLine(screen, x, y, l, 1, 0, outlineColor); // Top side
	DrawLine(screen, x, y + k - 1, l, 1, 0, outlineColor); // Bottom side
	for (int i = y + 1; i < y + k - 1; i++) DrawLine(screen, x + 1, i, l - 2, 1, 0, fillColor);
}


double** load_map() {
    int length, number_of_segments, fscanf_status;
    double **data;
    FILE *fptr;
    fptr = fopen("./map/platforms.txt","r");
    fscanf(fptr, "%d", &length);
    fscanf(fptr, "%d", &number_of_segments);
    data = (double**)malloc((2 + number_of_segments)*sizeof(double*));
    data[0] = (double*)malloc(4*sizeof(double)); // TODO: Remove 4 if possible
    data[0][0] = (double)length;
    data[1] = (double*)malloc(4*sizeof(double)); // TODO: Remove 4 if possible
    data[1][0] = (double)number_of_segments;
    for (int i = 0; i < number_of_segments; i++) {
        data[i+2] = (double*)malloc(4*sizeof(double));
        fscanf_status = fscanf(fptr, "%lf %lf %lf %lf", &data[i+2][0], &data[i+2][1], &data[i+2][2], &data[i+2][3]);
        if (fscanf_status != 4) { // We use the number of matches fscanf() returns to make sure we read exactly 4 number in each line.
            SDL_Log("Error reading the map file! Incorrect data in line %d!", i+3);
            fclose(fptr);
            exit(1);
        }
    }

    fgetc(fptr); // as scanf() leaves endlines in the buffer, we need to get rid of the end of the last line containing data
    if (!((fgetc(fptr) == '\n') || feof(fptr))) { // Now we check if the file ends either with an empty line or just after the end of the last line of data.
        SDL_Log("Error reading the map file! The file contains more lines than declared!"); // If there is anything besides that at the end of the file, we raise alarm.
        fclose(fptr);
        exit(1);
    }
    fclose(fptr);
    return data;
}

double** load_fairies () {}

double** load_stars () {}

void draw_map (SDL_Surface *screen, double map_offset, double map_length, int map_elements_count, double **map_elements, Uint32 outline_color, Uint32 fill_color) {
    int x;
    for (int i = 0; i < map_elements_count; i++) { // For each map element there is
        if (map_elements[i][0] < SCREEN_WIDTH && map_offset >= map_length - SCREEN_WIDTH) { // If it is one of the elements that fit within the first segment of map of length equal to screen width AND we're drawing the region of the map when map looping occurs
            x = map_elements[i][0] - map_offset + map_length; // Then let's cheat a little and say it lies beneath the map.
        }
        else x = (map_elements[i][0] - map_offset);

        if ((x <= SCREEN_WIDTH && x >= 0) // Left edge of the platform is within the screen
        || (x + map_elements[i][2] <= SCREEN_WIDTH && x + map_elements[i][2] >= 0) // Right edge of the platform is within the screen
        ) {
            // TODO: Modify this once the vertical offset is added
            DrawRectangle(screen, x, map_elements[i][1], map_elements[i][2], map_elements[i][3], outline_color, fill_color);
        }
    }
}

class Unicorn {
    // private
        // Immutable properties - only settable on instatiation.
        int dash_length, sprite_timer_threshold; // Duration of a single dash in ticks.
        // Variables
        bool sprite_phase, dashing;
        int dash_timer, sprite_timer; // Number of ticks since dashing started.
        SDL_Surface *spriteA_bmp = NULL, *spriteB_bmp = NULL;
        SDL_Texture *sprite_tex = NULL;

    public:
        int lives, width, height;
        float x, y; // Short names for coords because it's pretty clear anyway what these mean.
        double angle; // Current attitude.
        float x_velocity, y_velocity, y_acc;
        bool on_surface, double_jump_ready;
        Unicorn() {
            x = DEFAULT_X;
            y = DEFAULT_Y;
            x_velocity = STARTING_X_VELOCITY;
            y_velocity = 0.;
            y_acc = 0;
            angle = 0.;
            sprite_phase = false;
            double_jump_ready = true;
            lives = NUMBER_0F_LIVES;
            dash_length = 50; // The number of ticks the dash lasts.
            sprite_timer = 0;
            sprite_timer_threshold = 3;
            spriteA_bmp = SDL_LoadBMP("./resources/unicorn-spriteA.bmp");
            spriteB_bmp = SDL_LoadBMP("./resources/unicorn-spriteB.bmp");
            width = spriteA_bmp->w;
            height = spriteA_bmp->h;
        }
        int detect_collisions(double map_offset, double map_length, double map_elements_count, double **map_elements);
        bool die ();
        void reset();
        void jump();
        void dash();
        bool dashing_status();
        double dash_offset();
        SDL_Texture* sprite(SDL_Renderer* renderer);
} player;

SDL_Texture* Unicorn::sprite(SDL_Renderer* renderer) {
    if (!on_surface) return SDL_CreateTextureFromSurface(renderer, spriteB_bmp);
    else if (sprite_phase) sprite_tex = SDL_CreateTextureFromSurface(renderer, spriteA_bmp);
    else sprite_tex = SDL_CreateTextureFromSurface(renderer, spriteB_bmp);
    return sprite_tex;
}

bool Unicorn::die() {
    lives--;
    double_jump_ready = true;
    y -= 150; // To steer the player clear of the obstacle that has just killed them.
    if (lives == 0) {
        return true; // Should the game end?
    } else return false;
}

void Unicorn::reset() {
            x = DEFAULT_X;
            y = DEFAULT_Y;
            x_velocity = STARTING_X_VELOCITY;
            y_velocity = 0.;
            y_acc = 0;
            angle = 0.;
            sprite_phase = false;
            double_jump_ready = true;
            lives = NUMBER_0F_LIVES;
}

void Unicorn::jump() {
    if (dashing) return; // Cannot jump during a dash.

    if (on_surface) {
        y_acc = Y_ACC_CONST;
        y_velocity = JUMP_INITIAL_PUSH; // Initial push to escape collision detection that would otherwise snap us back to the ground.
    } else if (double_jump_ready && y_acc == 0.) {
        y_acc = Y_ACC_CONST;
        y_velocity = JUMP_INITIAL_PUSH; // Initial push to escape collision detection that would otherwise snap us back to the ground.
        double_jump_ready = false;
    }
    return;
}

void Unicorn::dash() {
    dashing = true;
    dash_timer = 0;
    double_jump_ready = true;
    return;
}

bool Unicorn::dashing_status() {
    return dashing;
}

double Unicorn::dash_offset() {
    return sin(dash_timer / dash_length * M_PI) * 15;
}

int Unicorn::detect_collisions(double map_offset, double map_length, double map_elements_count, double **map_elements) {
    // Return values:
    // 0 = no collision
    // 1 = standing on a platform
    // 2 = lethal collision
    // 3 = lethal collision and no more lives
    bool gameover = false;
    on_surface = false;

    if (y - height/2 > SCREEN_HEIGHT) { // Falling off the map.
        return 2 + die(); // die() returns true if the player has no more lives. This trock allows us to retur 2 if we died but can continue and 3 if we just lost our last life.
    }

    sprite_timer ++;
    if (sprite_timer > sprite_timer_threshold) { // If the threshold has been reached...
        sprite_phase ^= 1; // Flip the sprite phase bit.
        sprite_timer = 0; // Reset the timer.
    }

    if (dashing) {
        dash_timer += 1;
        if (dash_timer > dash_length) {
            dashing = false;
            dash_timer = 0;
        }
    }

    for (int i = 0; i < map_elements_count; i++) { // Check each and every element of the entire map
        bool horizontal_collision_condition = false, vertical_collision_condition = false;
        int platform_x;

        if (map_elements[i][0] < SCREEN_WIDTH && map_offset >= map_length - SCREEN_WIDTH) { // If it is one of the elements that fit within the first segment of map of length equal to screen width AND we're drawing the region of the map when map looping occurs
            platform_x = map_elements[i][0] - map_offset + map_length; // Then let's cheat a little and say it lies beneath the map.
        }
        else platform_x = (map_elements[i][0] - map_offset);

        // Check for deadly collisions
        if (
        (x + 0.4*width >= platform_x && x + 0.4*width <= platform_x + map_elements[i][2]) // IF the right edge of the player sprite is within the horizontal span of the platform
        || // AND/OR
        (x - 0.4*width >= platform_x && x - 0.4*width <= platform_x + map_elements[i][2]) // the right edge of the player sprite is within the horizontal span of the platform
        ) horizontal_collision_condition = true;

        if (
        (map_elements[i][1] >= y - 0.4*height && map_elements[i][1] <= y + 0.4*height)
        ||
        (map_elements[i][1] + map_elements[i][3] >= y - 0.4*height && map_elements[i][1] + map_elements[i][3] <= y + 0.4*height)
        ) vertical_collision_condition = true;

        if (vertical_collision_condition && horizontal_collision_condition) {
            return 2 + die();
        }

        // Check for bottom contact (i.e. if the unicorn stands on a platform)
        if (x + 0.4 * width >= platform_x
        && x - 0.4 * width <= platform_x + map_elements[i][2]
        && y + height/2 >= map_elements[i][1]
        && y + height/2 <= map_elements[i][1] + map_elements[i][3]
        ) {
            y = map_elements[i][1] - height/2;
            y_velocity = 0;
            on_surface = true;
            double_jump_ready = true;
        }

        // Check for frontal / top contact (i.e. if the unicorn has hit something and should die)
    }

    if (on_surface) return 1;
    if (gameover) return 3;
    return 0;
}

void toggle_cheaters_controls (bool *cheaters_controls, Unicorn *player) {
    if (*cheaters_controls) player->x = DEFAULT_X;
    *cheaters_controls ^= 1;
}

void new_game (Unicorn *player, double *map_offset) {
    player->reset();
    *map_offset = 0.;
    return;
}

// I'm using classes, so C++ compilation has to be used, but let's remember this trick for later.
// #ifdef __cplusplus
// extern "C"
// #endif
int main(int argc, char **argv) {
    SDL_Log("Starting Robot Unicorn Attack v0.2"); // Could use printf for logging, but SDL_Log feels so much more professional. ;)
	int t1, t2, frames, rc, map_length, map_elements_count, collision_status = 0;
	double delta, worldTime, fpsTimer, fps, ticker, map_offset;
	double **map_data, **map_elements;
	SDL_Event event;
	SDL_Surface *screen, *charset;
	SDL_Texture *scrtex; // Screen texture.
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Rect player_target_rect, rainbow_target_rect; // Player position where their sprite should be rendered.
	bool fullscreen = false; // TODO: Load this from config.
	bool cheaters_controls = true;
	bool quit = false;

	if(SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		printf("SDL_Init error: %s\n", SDL_GetError());
		return 1;
    }

	// fullscreen mode disabled for now
	if (fullscreen) rc = SDL_CreateWindowAndRenderer(0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP, &window, &renderer);
	else rc = SDL_CreateWindowAndRenderer(SCREEN_WIDTH, SCREEN_HEIGHT, 0, &window, &renderer);

	if(rc != 0) {
		SDL_Quit();
		printf("SDL_CreateWindowAndRenderer error: %s\n", SDL_GetError());
		return 1;
    }

	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "linear");
	SDL_RenderSetLogicalSize(renderer, SCREEN_WIDTH, SCREEN_HEIGHT);
	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    screen = SDL_CreateRGBSurface(0, SCREEN_WIDTH, SCREEN_HEIGHT, 32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
	scrtex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

	SDL_SetWindowTitle(window, "Robot Unicorn Attack");
	SDL_ShowCursor(SDL_DISABLE); // Hide cursor

	// load cs8x8.bmp
	charset = SDL_LoadBMP("./resources/cs8x8.bmp");
	if(charset == NULL) {
		printf("SDL_LoadBMP(cs8x8.bmp) error: %s\n", SDL_GetError());
		SDL_FreeSurface(screen);
		SDL_DestroyTexture(scrtex);
		SDL_DestroyWindow(window);
		SDL_DestroyRenderer(renderer);
		SDL_Quit();
		return 1;
    }
	SDL_SetColorKey(charset, true, 0x000000); // sets black as the transparent color for the bitmap loaded to charset

	SDL_Surface *rainbow_surf = SDL_LoadBMP("./resources/rainbow.bmp");
	SDL_Texture *rainbow = SDL_CreateTextureFromSurface(renderer, rainbow_surf);
	SDL_FreeSurface(rainbow_surf);

	char text[128];
	int color_black = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
	int color_red = SDL_MapRGB(screen->format, 0xFF, 0x00, 0x00);
	int color_green = SDL_MapRGB(screen->format, 0x00, 0xFF, 0x00);
	int color_blue = SDL_MapRGB(screen->format, 0x11, 0x11, 0xCC);
	int color_white = SDL_MapRGB(screen->format, 0xFF, 0xFF, 0xFF);
	int color_brown = SDL_MapRGB(screen->format, 0xA5, 0x2A, 0x2A);

	map_data = load_map();
    map_length = map_data[0][0]; // Only copy for convenience to have a more reasonable and informative variable name.
    map_elements_count = map_data[1][0]; // Same as above.
    map_elements = map_data + 2; // Same as above.

	t1 = SDL_GetTicks();
	frames = 0;
	fpsTimer = 0;
	fps = 0;
	worldTime = 0;
	ticker = 0;
	map_offset = 0.;

	while(!quit) {
		t2 = SDL_GetTicks();
		delta = (t2 - t1) * 0.001;
		worldTime += delta;
        fpsTimer += delta;
        ticker += (t2 - t1);
		if(fpsTimer > 0.5) {
			fps = frames * 2;
			frames = 0;
			fpsTimer -= 0.5;
        }
        if (ticker >= TICK_PERIOD) { // TODO Add for i in ticker % TICK_PERIOD to stay stable in extremely low framerates
            player.x_velocity = STARTING_X_VELOCITY * (1. + player.dashing_status() * 0.8); // Plus maybe later add acceleration over time.
            map_offset += player.x_velocity;
            if (map_offset >= map_length) {
                map_offset = fmod(map_offset, map_length);
            }
            player.x = DEFAULT_X + player.dash_offset();
            if (player.dashing_status()) player.y_velocity = 0;
            else player.y += player.y_velocity;
            if (cheaters_controls && player.y <= player.height/2) player.y = player.height/2;
            if (cheaters_controls && player.y + player.height/2 >= SCREEN_HEIGHT) player.y = SCREEN_HEIGHT - player.height/2;
            if (!cheaters_controls && !player.on_surface) {
                player.y_velocity = fmin(DRAG, player.y_velocity + GRAVITY + player.y_acc);
                if (player.y_velocity <= -(JUMP_STRENGTH * (1 + 0.3 * player.double_jump_ready))) player.y_acc = 0.; // If max velocity increase due to jumping achieved, stop accelerating. Multiplication by 1.3 to make the first jump a bit stronger than the second one.
            }
            player.angle = player.y_velocity / ((GRAVITY + DRAG) / 2) * 15;
            if (!cheaters_controls) collision_status = player.detect_collisions(map_offset, map_length, map_elements_count, map_elements);
            // TODO Handle collision status
            ticker = 0.;
        }
        t1 = t2;

        player_target_rect = {(int)(player.x - 75.), (int)(player.y - 59.), 150, 118}; // The rectangle in which player's sprite should be rendered.
        rainbow_target_rect = { // The rectangle in which the rainbow effect should be rendered when the player is dashing.
            (int)(player.x - 75. - 80.), // Upper left corner x coordinate.
            (int)(player.y - 59. + 0.5 * (player.height - 75.) + 20.), // Upper left corner y coordinate.
            player.width / 2 + 90., // Rectangle width.
            75 // Rectangle height.
        };
        SDL_RenderClear(renderer);
		SDL_FillRect(screen, NULL, color_black);
        draw_map(screen, map_offset, map_length, map_elements_count, map_elements, color_green, color_brown);
		DrawRectangle(screen, 4, 4, SCREEN_WIDTH - 8, 52, color_red, color_blue); // The info panel (points, FPS, lives etc.)
		sprintf(text, "Time elapsed = %.1lf s  %.0lf FPS (Frames Per Second)", worldTime, fps);
		DrawString(screen, screen->w / 2 - strlen(text) * 8 / 2, 10, text, charset);
		sprintf(text, "Lives left = %d", player.lives);
		DrawString(screen, screen->w / 2 - strlen(text) * 8 / 2, 26, text, charset);
		sprintf(text, "Esc - quit, A - jump, Z - dash, D - toggle cheater's controls.");
		DrawString(screen, screen->w / 2 - strlen(text) * 8 / 2, 42, text, charset);
        SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch); // Copy data from the screen surface to scrtex texture.
		SDL_RenderCopy(renderer, scrtex, NULL, NULL); // Render the scrtex onto the renderer.
        if (player.dashing_status()) {
            SDL_RenderCopyEx( // Render player's sprite onto the renderer.
                renderer,
                rainbow,
                NULL, // const SDL_Rect*        srcrect, NULL means copy the entire srcrect
                &rainbow_target_rect, // const SDL_Rect*        dstrect,
                player.angle,
                NULL, // Would take SDL_Point* center, but NULL means rotate about the center of the desitnation rectangle.
                SDL_FLIP_NONE
            );
        }
        if (
        SDL_RenderCopyEx( // Render player's sprite onto the renderer.
            renderer,
            player.sprite(renderer),
            NULL, // const SDL_Rect*        srcrect, NULL means copy the entire srcrect
            &player_target_rect, // const SDL_Rect*        dstrect,
            player.angle,
            NULL, // Would take SDL_Point* center, but NULL means rotate about the center of the desitnation rectangle.
            SDL_FLIP_NONE
        ) != 0 ) SDL_Log(SDL_GetError());
		SDL_RenderPresent(renderer);

		// handling of events (if there were any)
		while(SDL_PollEvent(&event)) {
			switch(event.type) { // Aways processing a single event, so I break whenever I can, i.e. when I know the event has been fully processed.
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_ESCAPE) {quit = true; break;}
					if (event.key.keysym.sym == SDLK_n) {new_game(&player, &map_offset); break;}
					if (event.key.keysym.sym == SDLK_d) {toggle_cheaters_controls(&cheaters_controls, &player); break;}
					if (cheaters_controls) {
                        if (event.key.keysym.sym == SDLK_UP) {player.y_velocity = -6.0; break;}
                        else if (event.key.keysym.sym == SDLK_DOWN) {player.y_velocity = 6.0; break;}
					} else {
                        if (event.key.keysym.sym == SDLK_z) {
                            player.jump();
                            break;
                        }
                        if (event.key.keysym.sym == SDLK_x) {
                            player.dash();
                            break;
                        }
					}
				case SDL_KEYUP:
                    if (cheaters_controls) {
                        player.y_velocity = 0.;
                        break;
					}
					else if (event.key.keysym.sym == SDLK_z) {
                        player.y_acc = 0.;
                        break;
					} else break;
				case SDL_QUIT:
					quit = true; break;
            }
        }
		frames++;
    };

	// freeing all surfaces
	SDL_FreeSurface(charset);
	SDL_FreeSurface(screen);
	SDL_DestroyTexture(scrtex);
	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);

	SDL_Quit();
	return 0;
};
