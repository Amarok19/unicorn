#define _USE_MATH_DEFINES
#define M_RAD 57.2958 // One radian equals 57.2958 degrees

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#define SCREEN_WIDTH	1280
#define SCREEN_HEIGHT	600
#define SPRITE_WIDTH_A 150
#define SPRITE_HEIGHT_A 118
#define SPRITE_WIDTH_B 150
#define SPRITE_HEIGHT_B 108
#define DEFAULT_X 225
#define DEFAULT_Y 300
#define GRAVITY .3
#define DRAG 10 // Max y velocity due to gravity. Named this way as it implements a simplified concept of drag balancing gravity.
#define JUMP_STRENGTH 10
#define NUMBER_0F_LIVES 3
#define TICK_PERIOD 3 // Number of milliseconds between ticks.

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
		};
};


void DrawSurface(SDL_Surface *screen, SDL_Surface *sprite, int x, int y) {
	SDL_Rect dest;
	dest.x = x - sprite->w / 2;
	dest.y = y - sprite->h / 2;
	dest.w = sprite->w;
	dest.h = sprite->h;
	SDL_BlitSurface(sprite, NULL, screen, &dest);
};


void DrawPixel(SDL_Surface *surface, double x, double y, Uint32 color) {
    x = round(x);
    y = round(y);
    if (x > SCREEN_WIDTH) {
        SDL_LogCritical(SDL_LOG_CATEGORY_VIDEO, "Tried to put a pixel at x = %d, y = %d. Screen width that is %d exceeded.", (int)x, (int)y, SCREEN_WIDTH);
        exit(1);
    }
    if (y > SCREEN_HEIGHT) {
        SDL_LogCritical(SDL_LOG_CATEGORY_VIDEO, "Tried to put a pixel at x = %d, y = %d. Screen height that is %d exceeded.", (int)x, (int)y, SCREEN_HEIGHT);
        exit(1);
    }
	int bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)surface->pixels + (int)y * surface->pitch + (int)x * bpp;
	*(Uint32 *)p = color;
};


// draw a vertical (when dx = 0, dy = 1) or horizontal (when dx = 1, dy = 0) line
void DrawLine(SDL_Surface *screen, int x, int y, int l, int dx, int dy, Uint32 color) {
	for(int i = 0; i < l; i++) {
		DrawPixel(screen, x, y, color);
		x += dx;
		y += dy;
		}
}


// draw a rectangle of size l by k
void DrawRectangle(SDL_Surface *screen, int x, int y, int l, int k,
                   Uint32 outlineColor, Uint32 fillColor) {
	int i;
	DrawLine(screen, x, y, k, 0, 1, outlineColor);
	DrawLine(screen, x + l - 1, y, k, 0, 1, outlineColor);
	DrawLine(screen, x, y, l, 1, 0, outlineColor);
	DrawLine(screen, x, y + k - 1, l, 1, 0, outlineColor);
	for (i = y + 1; i < y + k - 1; i++) DrawLine(screen, x + 1, i, l - 2, 1, 0, fillColor);
}

double local_map_height (double x, double **map_segments, int map_segments_count) {
    double y = 0.;
    for (int i = 0; i < map_segments_count; i++) {
        if (x >= map_segments[i][0] && x <= map_segments[i][1]) {
            for (int j = 0; j <= 4; j++) {
                y += map_segments[i][j+2] * pow(x - map_segments[i][0], 4. - (double)j);
            }
        }
    }
    if (y < 0) SDL_LogError(SDL_LOG_CATEGORY_ERROR, "For offset x = %f the map height is negative (y = %f).", x, y);
    return y;
}

void DrawMap(SDL_Surface *screen, double map_offset, double map_length, int map_segments_count, double **map_segments, Uint32 color) {
    int current_segment = -1; // -1 means we don't know which map segment to evaluate. We use this to avoid finding a matching segment for every pixel.
    for (double x = 1.; x <= SCREEN_WIDTH; x++) { // For each pixel across the screen width...
//        if (current_segment == -1) { // If we need to find a segment containing current offset...
//            for (int j = 0; j < map_segments_count; j++) {
//                if (x >= map_segments[j][0] && x <= map_segments[j][1]) { // If  current offset is within the domain of this segment...
//                    current_segment = j;
//                    break; // Found it - no need to look further.
//                }
//            }
//            if (current_segment == -1) exit(1); // If after searching the entire map we still don't have a segment containing current offset - exit the program.
//        }
        DrawPixel(screen, x, SCREEN_HEIGHT - local_map_height(fmod(x + map_offset, map_length), map_segments, map_segments_count), color);
        // SDL_Log("Look, Ma, I'm drawing a green dot at x = %f, y = %f!!!", x, SCREEN_HEIGHT - local_map_height(fmod(x + map_offset, map_length), map_segments, map_segments_count));
//        if (x >= map_segments[i][1]) current_segment = -1; // We've reached the end of the domain of this segment. Time to look for the next one. Assume they may not be in consecutive sequence.
    }
}

SDL_Point graphic_rotate(SDL_Point coords, SDL_Point pivot, double angle) {
    // Called "graphic" rotate because it adheres to how coordinates work in graphics-related code:
    // x works as usual, but y increases downwards.
    // angle must be in radians.
    SDL_Point result;
    double x, y, x_rot, y_rot;
    angle = angle / M_RAD;
//    x = (double)(coords.x - pivot.x);
//    y = (double)(coords.y - pivot.y);
    x = (double)coords.x;
    y = (double)coords.y;
    x_rot = x * cos(angle);
    y_rot = y * (1 - sin((-1)*angle));
    result.x = (int)x_rot + pivot.x;
    result.y = (int)y_rot + pivot.y;
    SDL_Log("%d %d %d %d", coords.x, coords.y, result.x, result.y);
    return result;
}

double** load_map() {
    int length, number_of_segments;
    double **data;
    FILE *fptr;
    fptr = fopen("./map.txt","r");
    fscanf(fptr, "%d", &length);
    fscanf(fptr, "%d", &number_of_segments);
    data = (double**)malloc((2 + number_of_segments)*sizeof(double*));
    data[0] = (double*)malloc(7*sizeof(double));
    data[0][0] = (double)length;
    data[1] = (double*)malloc(7*sizeof(double));
    data[1][0] = (double)number_of_segments;
    for (int i = 0; i < number_of_segments; i++) {
        data[i+2] = (double*)malloc(7*sizeof(double));
        fscanf(fptr, "%lf %lf %lf %lf %lf %lf %lf", &data[i+2][0], &data[i+2][1], &data[i+2][2], &data[i+2][3], &data[i+2][4], &data[i+2][5], &data[i+2][6]);
    }
    fclose(fptr);
    return data;
}

class Unicorn {
    // private
        // Immutable properties - only settable on instatiation.
        int front_hooves_pos_A_x = 132 - (SPRITE_WIDTH_A / 2),
            front_hooves_pos_A_y = 107 - (SPRITE_HEIGHT_A / 2),
            rear_hooves_pos_A_x = 26 - (SPRITE_WIDTH_A / 2),
            rear_hooves_pos_A_y = 107 - (SPRITE_HEIGHT_A / 2),
            nose_pos_A_x = 150 - (SPRITE_WIDTH_A / 2),
            nose_pos_A_y = 55 - (SPRITE_HEIGHT_A / 2),
            front_hooves_pos_B_x = 115 - (SPRITE_WIDTH_B / 2),
            front_hooves_pos_B_y = 118 - (SPRITE_HEIGHT_B / 2),
            rear_hooves_pos_B_x = 43 - (SPRITE_WIDTH_B / 2),
            rear_hooves_pos_B_y = 118 - (SPRITE_HEIGHT_B / 2),
            nose_pos_B_x = 150- (SPRITE_WIDTH_B / 2),
            nose_pos_B_y = 55 - (SPRITE_HEIGHT_B / 2);
        float dash_length;
        // Variables
        int lives;
        bool sprite_phase, dashing;
        SDL_Texture *sprite_tex = NULL;

    public:
        SDL_Surface *spriteA_bmp = NULL, *spriteB_bmp = NULL; // DEBUG Make it private again later on
        float x, y; // Current attitude. Short names for coords because it's pretty clear anyway what these mean. Not using coords struct intentionally.
        double angle;
        float x_velocity, y_velocity, x_movement; // x_movement is for cheater's controls only. It's player's velocity w.r.t. the screen.
        bool on_surface, double_jump_ready;
        Unicorn() {
            x = DEFAULT_X;
            y = DEFAULT_Y;
            x_velocity = 10.;
            y_velocity = 0.;
            x_movement = 0.;
            angle = 0.;
            sprite_phase = false;
            double_jump_ready = true;
            lives = NUMBER_0F_LIVES;
            dash_length = 50; // The number of ticks the dash lasts.
            spriteA_bmp = SDL_LoadBMP("./resources/unicorn-spriteA.bmp");
            spriteB_bmp = SDL_LoadBMP("./resources/unicorn-spriteB.bmp");
        }
        SDL_Point get_front_hooves_pos();
        SDL_Point get_rear_hooves_pos();
        SDL_Point get_nose_pos();
        bool die ();
        void jump();
        void dash();
        SDL_Texture* sprite(SDL_Renderer* renderer);
} player;

SDL_Texture* Unicorn::sprite(SDL_Renderer* renderer) {
    if (sprite_phase) sprite_tex = SDL_CreateTextureFromSurface(renderer, spriteA_bmp);
    else sprite_tex = SDL_CreateTextureFromSurface(renderer, spriteB_bmp);
    return sprite_tex;
}

bool Unicorn::die() {
    lives--;
    double_jump_ready = true;
    y = DEFAULT_Y;
    if (lives == 0) {
        return true; // Should the game end?
    } else return false;
}

void Unicorn::jump() {
    if (on_surface) {
        y_velocity = -JUMP_STRENGTH;
    } else if (double_jump_ready) {
        y_velocity = -JUMP_STRENGTH;
        double_jump_ready = false;
    }
    return;
}

void Unicorn::dash() {
    dashing = true;
    double_jump_ready = true;
    return;
}

SDL_Point Unicorn::get_nose_pos() {
    int nose_x, nose_y;
    if (sprite_phase) {
        nose_x = nose_pos_A_x;
        nose_y = nose_pos_A_y;
    } else {
        nose_x = nose_pos_B_x;
        nose_y = nose_pos_B_y;
    }
    SDL_Point coords = {nose_x, nose_y};
    SDL_Point pivot = {x, y};
    return graphic_rotate(coords, pivot, angle);
}

SDL_Point Unicorn::get_front_hooves_pos() {
    int hooves_x, hooves_y;
    if (sprite_phase) {
        hooves_x = front_hooves_pos_A_x;
        hooves_y = front_hooves_pos_A_y;
    } else {
        hooves_x = front_hooves_pos_B_x;
        hooves_y = front_hooves_pos_B_y;
    }
    SDL_Point coords = {hooves_x, hooves_y};
    SDL_Point pivot = {x, y};
    return graphic_rotate(coords, pivot, angle);
}

SDL_Point Unicorn::get_rear_hooves_pos() {
    int hooves_x, hooves_y;
    if (sprite_phase) {
        hooves_x = rear_hooves_pos_A_x;
        hooves_y = rear_hooves_pos_A_y;
    } else {
        hooves_x = rear_hooves_pos_B_x;
        hooves_y = rear_hooves_pos_B_y;
    }
    SDL_Point coords = {hooves_x, hooves_y};
    SDL_Point pivot = {x, y};
    return graphic_rotate(coords, pivot, angle);
}

void toggle_cheaters_controls (bool *cheaters_controls, Unicorn *player) {
    if (*cheaters_controls) player->x = DEFAULT_X;
    *cheaters_controls ^= 1;
}

// I'm using classes, so C++ compilation has to be used, but let's remember this trick for later.
// #ifdef __cplusplus
// extern "C"
// #endif
int main(int argc, char **argv) {
    SDL_Log("Starting Robot Unicorn Attack v0.1"); // Could use printf for logging, but SDL_Log feels so much more professional. ;)
	int t1, t2, frames, rc, map_length, map_segments_count;
	double delta, worldTime, fpsTimer, fps, ticker, map_offset;
	double **map_data, **map_segments;
	SDL_Event event;
	SDL_Surface *screen, *charset;
	SDL_Texture *scrtex; // Screen texture.
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Rect player_target_rect;
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

    // DEBUG
	SDL_Surface* debug_surface = NULL;
	debug_surface = SDL_LoadBMP("./resources/unicorn-spriteA.bmp");
    if(debug_surface == NULL) {
        SDL_Log("Error initializing debug_surface!");
	}
	SDL_Texture* debug_texture = NULL;
//	debug_texture = SDL_CreateTextureFromSurface(renderer, debug_surface);
    debug_texture = SDL_CreateTextureFromSurface(renderer, player.spriteA_bmp);
	if(debug_texture == NULL) {
        SDL_Log("Error initializing debug_texture!");
	}
	// DEBUG

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

	char text[128];
	int color_black = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
	int color_red = SDL_MapRGB(screen->format, 0xFF, 0x00, 0x00);
	int color_green = SDL_MapRGB(screen->format, 0x00, 0xFF, 0x00);
	int color_blue = SDL_MapRGB(screen->format, 0x11, 0x11, 0xCC);
	int color_white = SDL_MapRGB(screen->format, 0xFF, 0xFF, 0xFF);

	map_data = load_map();
    map_length = map_data[0][0]; // Only copy for convenience to have a more reasonable and informative variable name.
    map_segments_count = map_data[1][0]; // Same as above.
    map_segments = map_data + 2;

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
        if (ticker >= TICK_PERIOD) {
            map_offset += player.x_velocity;
            if (map_offset >= map_length) {
                map_offset = fmod(map_offset, map_length);
            }
            player.x += player.x_movement;
            player.y += player.y_velocity;
            if (!cheaters_controls && !player.on_surface) {
                player.y_velocity = fmin(DRAG, player.y_velocity + GRAVITY);
            }
            player.angle = player.y_velocity / ((GRAVITY + DRAG) / 2) * 15;
            if (player.y + 59 >= SCREEN_HEIGHT) { // Replace with dynamic sprite height.
                player.on_surface = true;
                player.double_jump_ready = true;
                player.y = SCREEN_HEIGHT - 59;
                player.y_velocity = 0;
            } else {
                player.on_surface = false;
            }
            ticker = 0.;
        }
        t1 = t2;

        player_target_rect = {(int)(player.x - 75.), (int)(player.y - 59.), 150, 118};
        SDL_RenderClear(renderer);
		SDL_FillRect(screen, NULL, color_black);
		DrawMap(screen, map_offset, map_length, map_segments_count, map_segments, color_green);
		DrawRectangle(screen, 4, 4, SCREEN_WIDTH - 8, 36, color_red, color_blue); // The info panel (points, FPS, lives etc.)
		sprintf(text, "Time elapsed = %.1lf s  %.0lf FPS (Frames Per Second)", worldTime, fps);
		DrawString(screen, screen->w / 2 - strlen(text) * 8 / 2, 10, text, charset);
		sprintf(text, "Esc - quit, A - jump, Z - dash, D - toggle cheater's controls.");
		DrawString(screen, screen->w / 2 - strlen(text) * 8 / 2, 26, text, charset);
		SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch); // Copy data from the screen surface to scrtex texture.
		SDL_RenderCopy(renderer, scrtex, NULL, NULL); // Render the scrtex onto the renderer.
        if (
        SDL_RenderCopyEx( // Render player's sprite onto the renderer.
            renderer,
            player.sprite(renderer), // SDL_Texture* texture ;; Source texture // DEBUG
            NULL, // const SDL_Rect*        srcrect, NULL means copy the entire srcrect
            &player_target_rect, // const SDL_Rect*        dstrect,
            // player.angle, // DEBUG
            0.,
            NULL, // Would take SDL_Point* center, but NULL means rotate about the center of the desitnation rectangle.
            SDL_FLIP_NONE
        ) != 0 ) SDL_Log(SDL_GetError());
        // DEBUG
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        // Nose
        player_target_rect = {player.get_nose_pos().x-2, player.get_nose_pos().y-2, 4, 4};
        SDL_RenderDrawRect(renderer, &player_target_rect);
        // Front hooves
        player_target_rect = {(int)player.get_front_hooves_pos().x-2, (int)player.get_front_hooves_pos().y-2, 4, 4};
        SDL_RenderDrawRect(renderer, &player_target_rect);
        // Rear hooves
        player_target_rect = {(int)player.get_rear_hooves_pos().x-2, (int)player.get_rear_hooves_pos().y-2, 4, 4};
        SDL_RenderDrawRect(renderer, &player_target_rect);
        // Null rotation test
        SDL_SetRenderDrawColor(renderer, 0, 0, 255, 255);
        SDL_Point player_coords = {player.x, player.y};
        SDL_Point null_point = {50, 0};
        player_target_rect = {graphic_rotate(null_point, player_coords, player.angle).x, graphic_rotate(null_point, player_coords, player.angle).y, 4, 4};
        SDL_RenderDrawRect(renderer, &player_target_rect);
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
        // Player's coordinates.
        player_target_rect = {player.x-2, player.y-2, 4, 4};
        SDL_RenderDrawRect(renderer, &player_target_rect);
		// Reset the draw color
		SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
		// /DEBUG
		SDL_RenderPresent(renderer);

		// handling of events (if there were any)
		while(SDL_PollEvent(&event)) {
			switch(event.type) { // Aways processing a single event, so I break whenever I can, i.e. when I know the event has been fully processed.
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_ESCAPE) {quit = true; break;}
					if (event.key.keysym.sym == SDLK_d) {toggle_cheaters_controls(&cheaters_controls, &player); break;}
					if (cheaters_controls) {
                        if (event.key.keysym.sym == SDLK_UP) {player.y_velocity = -2.0; break;}
                        else if (event.key.keysym.sym == SDLK_DOWN) {player.y_velocity = 2.0; break;}
                        else if (event.key.keysym.sym == SDLK_LEFT) {player.x_movement = -2.0; break;}
                        else if (event.key.keysym.sym == SDLK_RIGHT) {player.x_movement = 2.0; break;}
					} else {
                        if (event.key.keysym.sym == SDLK_a) {
                            player.jump();
                            break;
                        }
                        if (event.key.keysym.sym == SDLK_z) {
//                            if (player.get_front_hooves_pos()->y <= player.get_rear_hooves_pos()->y) {
//                                player.dash();
//                            } // If we are on a flat surface or leaning to the front, we can dash. Or if we are in the air. Rewrite this # TODO
                            break;
                        }
					}
				case SDL_KEYUP:
                    if (cheaters_controls) {
                        player.x_movement = 0.;
                        player.y_velocity = 0.;
                        break;
					}
					else {
                        break;
					}
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

    SDL_FreeSurface(debug_surface); // DEBUG
	SDL_DestroyTexture(debug_texture); // DEBUG

	SDL_Quit();
	return 0;
};
