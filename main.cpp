#define _USE_MATH_DEFINES
#define M_DEG 0.0174533 // 1 angular degree in radians

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>

#define SCREEN_WIDTH	640
#define SCREEN_HEIGHT	480
#define DEFAULT_X 20
#define gravity .3
#define drag 1.2 // Max y velocity due to gravity. Named this way as it implements a simplified concept of drag balancing gravity.

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


void DrawPixel(SDL_Surface *surface, int x, int y, Uint32 color) {
	int bpp = surface->format->BytesPerPixel;
	Uint8 *p = (Uint8 *)surface->pixels + y * surface->pitch + x * bpp;
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

struct coordinates {
    float x, y;
};

coordinates graphic_rotate(coordinates coords, coordinates pivot, float angle) {
    // Called "graphic" rotate because it adheres to how coordinates work in graphics-related code:
    // x works as usual, but y increases downwards.
    // angle must be in radians.
    coordinates result;
    float x, y, x_rot, y_rot;
    x = coords.x - pivot.x;
    y = coords.y - pivot.y;
    x_rot = x * cos(angle);
    y_rot = y * (1 - sin(angle));
    result.x = x_rot + pivot.x;
    result.y = y_rot + pivot.y;
    return result;
}

class Unicorn {
    // private
        // Immutable properties - only settable on instatiation.
        int init_front_hooves_pos_A, init_rear_hooves_pos_A, init_front_hooves_pos_B, init_rear_hooves_pos_B,
            init_nose_pos_A, init_nose_pos_B;
        // Variables
        int lives;
        bool double_jump_ready, sprite_phase;
        SDL_Surface *spriteA, *spriteB;

    public:
        float angle, x, y; // Current attitude. Short names for coords because it's pretty clear anyway what these mean. Not using coords struct intentionally.
        float x_velocity = 0, y_velocity = 0;
        bool on_surface;
        Unicorn() {
            x = DEFAULT_X;
            y = 60;
            angle = 0;
            sprite_phase = false;
            double_jump_ready = true;
            lives = 3;
            spriteA = SDL_LoadBMP("./resources/unicorn-spriteA.bmp");
            spriteB = SDL_LoadBMP("./resources/unicorn-spriteB.bmp");
        }
        coordinates get_front_hooves_pos();
        coordinates get_rear_hooves_pos();
        coordinates get_nose_pos();
        void jump();
        void dash();
        SDL_Surface* sprite();
} player;

void toggle_cheaters_controls (bool *cheaters_controls, Unicorn *player) {
    if (*cheaters_controls) player->x = DEFAULT_X;
    *cheaters_controls ^= 1;
}

SDL_Surface* Unicorn::sprite() {
    if (sprite_phase) return spriteA;
    else return spriteB;
}

void Unicorn::jump() {
    if (on_surface) {
        y_velocity -= 10;
    } else if (double_jump_ready) {
        y_velocity -= 10;
        double_jump_ready = false;
    }
    return;
}

void Unicorn::dash() {
    return;
}


// I'm using classes, so C++ compilation has to be used, but let's remember this trick for later.
// #ifdef __cplusplus
// extern "C"
// #endif
int main(int argc, char **argv) {
    SDL_Log("Starting Robot Unicorn Attack v0.1"); // Could use printf for logging, but SDL_Log feels so much more professional. ;)
	int t1, t2, frames, rc;
	double delta, worldTime, fpsTimer, fps;
	SDL_Event event;
	SDL_Surface *screen, *charset;
	SDL_Surface *eti;
	SDL_Texture *scrtex; // Screen texture.
	SDL_Window *window;
	SDL_Renderer *renderer;
	bool fullscreen = false; // TODO: Load this from config.
	bool cheaters_controls = true;
	bool quit = false;

	if(SDL_Init(SDL_INIT_EVERYTHING) != 0) {
		printf("SDL_Init error: %s\n", SDL_GetError());
		return 1;
    }

	// fullscreen mode
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

	eti = SDL_LoadBMP("./eti.bmp");
	if(eti == NULL) {
		printf("SDL_LoadBMP(eti.bmp) error: %s\n", SDL_GetError());
		SDL_FreeSurface(charset);
		SDL_FreeSurface(screen);
		SDL_DestroyTexture(scrtex);
		SDL_DestroyWindow(window);
		SDL_DestroyRenderer(renderer);
		SDL_Quit();
		return 1;
    };

	char text[128];
	int color_black = SDL_MapRGB(screen->format, 0x00, 0x00, 0x00);
	int color_green = SDL_MapRGB(screen->format, 0x00, 0xFF, 0x00);
	int color_red = SDL_MapRGB(screen->format, 0xFF, 0x00, 0x00);
	int color_blue = SDL_MapRGB(screen->format, 0x11, 0x11, 0xCC);

	t1 = SDL_GetTicks();
	frames = 0;
	fpsTimer = 0;
	fps = 0;
	worldTime = 0;

	while(!quit) {
		t2 = SDL_GetTicks();

		// w tym momencie t2-t1 to czas w milisekundach,
		// jaki uplyna³ od ostatniego narysowania ekranu
		// delta to ten sam czas w sekundach
		// here t2-t1 is the time in milliseconds since
		// the last screen was drawn
		// delta is the same time in seconds
		delta = (t2 - t1) * 0.001;
		t1 = t2;
		worldTime += delta;

		SDL_FillRect(screen, NULL, color_black);

//		DrawSurface(screen, eti,
//		            SCREEN_WIDTH / 2 + sin(distance) * SCREEN_HEIGHT / 3,
//			    SCREEN_HEIGHT / 2 + cos(distance) * SCREEN_HEIGHT / 3);

        DrawSurface(screen, player.sprite(), player.x, player.y);

		fpsTimer += delta;
		if(fpsTimer > 0.5) {
			fps = frames * 2;
			frames = 0;
			fpsTimer -= 0.5;
        };

		// info text
		DrawRectangle(screen, 4, 4, SCREEN_WIDTH - 8, 36, color_red, color_blue);
		//            "template for the second project, elapsed time = %.1lf s  %.0lf frames / s"
		sprintf(text, "Szablon drugiego zadania, czas trwania = %.1lf s  %.0lf klatek / s", worldTime, fps);
		DrawString(screen, screen->w / 2 - strlen(text) * 8 / 2, 10, text, charset);
		//	      "Esc - exit, \030 - faster, \031 - slower"
		sprintf(text, "Esc - wyjscie, \030 - przyspieszenie, \031 - zwolnienie");
		DrawString(screen, screen->w / 2 - strlen(text) * 8 / 2, 26, text, charset);

		SDL_UpdateTexture(scrtex, NULL, screen->pixels, screen->pitch);
		// SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, scrtex, NULL, NULL);
		SDL_RenderPresent(renderer);

		player.x += player.x_velocity;
		player.y += player.y_velocity;
		if (!cheaters_controls && !player.on_surface) {
            player.y_velocity = fmin(drag, player.y_velocity + gravity);
        }
        if (player.y + 118 >= SCREEN_HEIGHT) { // Replace with dynamic sprite height.
            player.y = SCREEN_HEIGHT - 118;
            player.on_surface = true;
            player.y_velocity = 0;
        } else {
            player.on_surface = false;
        }

		// handling of events (if there were any)
		while(SDL_PollEvent(&event)) {
			switch(event.type) { // Aways processing a single event, so I break whenever I can, i.e. when I know the event has been fully processed.
				case SDL_KEYDOWN:
					if (event.key.keysym.sym == SDLK_ESCAPE) {quit = true; break;}
					if (event.key.keysym.sym == SDLK_d) {toggle_cheaters_controls(&cheaters_controls, &player); break;}
					if (cheaters_controls) {
                        if (event.key.keysym.sym == SDLK_UP) {player.y_velocity = -2.0; break;}
                        else if (event.key.keysym.sym == SDLK_DOWN) {player.y_velocity = 2.0; break;}
                        else if (event.key.keysym.sym == SDLK_LEFT) {player.x_velocity = -2.0; break;}
                        else if (event.key.keysym.sym == SDLK_RIGHT) {player.x_velocity = 2.0; break;}
					} else {
                        if (event.key.keysym.sym == SDLK_a) {
                            player.jump();
                            break;
                        }
                        if (event.key.keysym.sym == SDLK_z) {
                            player.dash();
                            break;
                        }
					}
				case SDL_KEYUP:
                    if (cheaters_controls) {
                        player.x_velocity = 0.;
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

	SDL_Quit();
	return 0;
};
