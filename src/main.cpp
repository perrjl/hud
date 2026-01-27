#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <cstdint>

#define CLAY_IMPLEMENTATION
extern "C" {
	#include "clay_renderer_SDL3.h"
}

#define HUDInternal static

struct HUDData {
	SDL_Window* window;
	SDL_Renderer* renderer;
	Clay_Context* clay_context;
	float mouse_x;
	float mouse_y;
	int viewport_w;
	int viewport_h;
	int user_x;
	int user_y;
	// uint64_t delta_ns;
};

HUDInternal uint64_t start; // Set in main() before execution of the program loop

// Event processing function.
// Will attempt to process an event at least once every 100 ms.
// If there are no input events recently, the program will be waiting on this function for at least 100ms.
bool ProcessEvents(HUDData& data) {
	static SDL_Event events[1024];
	static uint64_t frames = 0;
	static uint64_t last_frame_time = SDL_GetTicksNS();
	static uint64_t time_eventless = SDL_GetTicksNS();

	SDL_Event event;
	int total_events = 0; // If there are no events, none will be processed.
	uint64_t current_frame_time = SDL_GetTicksNS();
	Clay_SetPointerState((Clay_Vector2) { data.mouse_x, data.mouse_y }, false); // Reset mouse pointer state

	uint64_t delta_ns = current_frame_time - last_frame_time;
	uint64_t ns_since_last_event = current_frame_time - time_eventless;

	// Get events immediately if they are recent (5 seconds), else wait for the next event
	// Handle up to 1024 events per frame, which are popped off the event queue
	if (ns_since_last_event < 5e9) {
		// SDL_Log("Input Processing 1 (Live)\n");
		// Update the event queue once per frame when input processing, better for performance and low latency input handling
		SDL_PumpEvents();
		total_events = SDL_PeepEvents(events, 1024, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
		if (total_events > 0) time_eventless = SDL_GetTicksNS();
	} else {
		// SDL_Log("Input Processing 2 (Awaiting)\n");
		// If inactive, poll every 100 ms for an event, and if an event was received only process one event this frame
		if (SDL_WaitEventTimeout(events, 100)) {
			total_events = 1 + SDL_PeepEvents(&events[1], 1023, SDL_GETEVENT, SDL_EVENT_FIRST, SDL_EVENT_LAST);
			time_eventless = SDL_GetTicksNS();
		}
	}

	// Process all events that have been fetched so far
	for (int i = 0; i < total_events; ++i) {
		event = events[i];
		if (event.type == SDL_EVENT_KEY_DOWN) {
			SDL_Keycode key = event.key.key;
			if (key == SDLK_W) {
				data.user_y -= 1;
			} else if (key == SDLK_S) {
				data.user_y += 1;
			} else if (key == SDLK_A) {
				data.user_x -= 1;
			} else if (key == SDLK_D) {
				data.user_x += 1;
			}
		} else if (event.type == SDL_EVENT_QUIT || event.key.key == SDLK_ESCAPE) {
			double seconds = (double)(current_frame_time - start) / 1.0e9;
			SDL_Log("FPS: %lf, Total Frames: %llu, Time Elapsed: %lf sec", frames / seconds, frames, seconds);
			return false;
		} else if (event.type == SDL_EVENT_WINDOW_RESIZED) {
			if (!SDL_GetRenderOutputSize(data.renderer, &data.viewport_w, &data.viewport_h)) {
				SDL_Log("Error: %s\n", SDL_GetError());
				return false;
			}
			Clay_SetLayoutDimensions((Clay_Dimensions) { (float)data.viewport_w, (float)data.viewport_h });
		} else if (event.type == SDL_EVENT_MOUSE_MOTION) {
			SDL_GetMouseState(&data.mouse_x, &data.mouse_y);
			Clay_SetPointerState((Clay_Vector2) { data.mouse_x, data.mouse_y }, event.motion.state & SDL_BUTTON_LMASK);
		} else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
			SDL_GetMouseState(&data.mouse_x, &data.mouse_y);
			Clay_SetPointerState((Clay_Vector2) { data.mouse_x, data.mouse_y }, true);
		} else if (event.type == SDL_EVENT_MOUSE_WHEEL) {
			// if implementing scrolling containers, update with change in relative position
			// Clay_UpdateScrollContainers(true, (Clay_Vector2) { event.wheel.x, event.wheel.y }, (double)delta_ns);
		}
	}

	frames++;
	last_frame_time = current_frame_time;

	return true;
}

// Callback for errors inside Clay
void HandleClayErrors(Clay_ErrorData errorData) {
	// See the Clay_ErrorData struct for more information
	SDL_Log("Error: %s\n", errorData.errorText.chars);
	switch(errorData.errorType) {
		default:
		break;
	}
}

void HandleInteraction(Clay_ElementId elementId, Clay_PointerData pointerInfo, void *userData) {
	// Pointer state allows you to detect mouse down / hold / release
	if (pointerInfo.state == CLAY_POINTER_DATA_PRESSED_THIS_FRAME) {
		// Do some click handling
		SDL_Log("Clicked!\n");
		bool* swap = (bool*)userData;
		*swap = !(*swap);
	}
}

Clay_Dimensions SDL_MeasureText(Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
	TTF_Font **fonts = (TTF_Font**)userData;
	TTF_Font *font = fonts[0];
	int width, height;

	TTF_SetFontSize(font, config->fontSize);
	if (!TTF_GetStringSize(font, text.chars, text.length, &width, &height)) {
		SDL_LogError(SDL_LOG_CATEGORY_ERROR, "Failed to measure text: %s", SDL_GetError());
	}
	return (Clay_Dimensions) { (float)width, (float)height };
}

int main() {
	if (!SDL_Init(SDL_INIT_VIDEO)) {
		SDL_Log("Error: %s\n", SDL_GetError());
		return -1;
	}

	if (!TTF_Init()) {
		SDL_Log("Error: %s", SDL_GetError());
		return -1;
	}

	// Create SDL and Clay contexts
	HUDData data = {
		.window = NULL,
		.renderer = NULL,
		.clay_context = NULL,
		.mouse_x = 0,
		.mouse_y = 0,
		.viewport_w = 0,
		.viewport_h = 0,
		.user_x = 200,
		.user_y = 150,
	};

	{
		SDL_Rect display_dimensions;
		if (!SDL_GetDisplayBounds(SDL_GetPrimaryDisplay(), &display_dimensions)) {
			SDL_Log("Error: %s\n", SDL_GetError());
			SDL_Quit();
			return -1;
		}

		float fwindow_w = (float)display_dimensions.w;
		float fwindow_h = (float)display_dimensions.h;
		// float aspect_ratio = fwindow_w / fwindow_h;
		if (!SDL_CreateWindowAndRenderer("HUD", display_dimensions.w, display_dimensions.h, SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN, &data.window, &data.renderer)) {
			SDL_Log("Error: %s\n", SDL_GetError());
			SDL_Quit();
			return -1;
		}

		size_t total_clay_memory = Clay_MinMemorySize();
		void* memory = calloc(total_clay_memory, 1);
		if (memory == NULL) {
			SDL_Log("Error: failled to calloc memory\n");
			SDL_Quit();
			return -1;
		}

		Clay_Arena arena = Clay_CreateArenaWithCapacityAndMemory(total_clay_memory, memory);

		data.clay_context = Clay_Initialize(
			arena,
			(Clay_Dimensions) { fwindow_w, fwindow_h },
			(Clay_ErrorHandler) { .errorHandlerFunction = HandleClayErrors }
		);
		if (data.clay_context == NULL) {
			SDL_Log("Error: failed to initialize Clay\n");
			SDL_Quit();
			return -1;
		}

		if (!SDL_GetRenderOutputSize(data.renderer, &data.viewport_w, &data.viewport_h)) {
			SDL_Log("Error: %s\n", SDL_GetError());
			SDL_Quit();
			return -1;
		}
		Clay_SetLayoutDimensions((Clay_Dimensions) { (float)data.viewport_w, (float)data.viewport_h });

		SDL_GetMouseState(&data.mouse_x, &data.mouse_y);
		Clay_SetPointerState((Clay_Vector2) { data.mouse_x, data.mouse_y }, false);
	}

	TTF_Font* fonts[] = { TTF_OpenFont("assets/OpenSans-Regular.ttf", 16) };
	TTF_TextEngine* engine = TTF_CreateRendererTextEngine(data.renderer);

	Clay_SDL3RendererData render_data = {
		.renderer = data.renderer,
		.textEngine = engine,
		.fonts = fonts,
	};

	start = SDL_GetTicksNS();

	Clay_SetMeasureTextFunction(SDL_MeasureText, fonts);

	bool swapped[2] = { false, false };

	while (true) {
		if (!ProcessEvents(data)) {
			SDL_Quit();
			return 0;
		}

		// SDL_RenderClear(data.renderer);

		Clay_BeginLayout();

		// Note that regular C statements are permitted inside the layout declaration,
		// just not inside the member struct declarations
		CLAY(CLAY_ID("Box"), {
		 	.layout = {
				.sizing = { CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0) },
				.layoutDirection = CLAY_LEFT_TO_RIGHT,
			},
			.backgroundColor = (Clay_Color) { .r = 173, .g = 216, .b = 230, .a = 255 },
			.cornerRadius = CLAY_CORNER_RADIUS(4),
		}) {
			CLAY(CLAY_ID("Top"), {
				.layout = {
					.sizing = { .width = CLAY_SIZING_PERCENT(1.0), .height = CLAY_SIZING_PERCENT(0.1) },
					.padding = CLAY_PADDING_ALL(16),
					.childGap = 16,
					.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
					.layoutDirection = CLAY_LEFT_TO_RIGHT
				},
				.backgroundColor = (Clay_Color) { .r = 192, .g = 192, .b = 192, .a = 255 },
				.cornerRadius = CLAY_CORNER_RADIUS(4),
			}) {
				CLAY(CLAY_ID("TopLeft"), {
					.layout = {
						.sizing = { .width = CLAY_SIZING_PERCENT(0.5), .height = CLAY_SIZING_PERCENT(1.0) },
						.padding = CLAY_PADDING_ALL(16),
						.childGap = 16,
						.childAlignment = { CLAY_ALIGN_X_LEFT, CLAY_ALIGN_Y_TOP },
						.layoutDirection = CLAY_LEFT_TO_RIGHT
					},
					.backgroundColor = (Clay_Color) { .r = 192, .g = 192, .b = 192, .a = 255 },
					.cornerRadius = CLAY_CORNER_RADIUS(4),
				}) {
					CLAY(CLAY_ID("BoxTL"), {
						.layout = {
							.sizing = { .width = CLAY_SIZING_PERCENT(0.2), .height = CLAY_SIZING_PERCENT(1.0) },
							.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER }, // This alignment must be set to actually align the text
						},
						.backgroundColor = (Clay_Color) { .r = 156, .g = 156, .b = 156, .a = 255 },
						.cornerRadius = CLAY_CORNER_RADIUS(4),
					}) {
						Clay_OnHover(HandleInteraction, &swapped[0]); // Clicks should only be registered in the relevant UI element
						CLAY_TEXT(swapped[0] ? CLAY_STRING("Swapped") : CLAY_STRING("Click me!"), CLAY_TEXT_CONFIG({
							.textColor = (Clay_Color) { .r = 0, .g = 0, .b = 0, .a = 255 },
							.fontId = 0,
							.fontSize = 16,
							// .textAlignment = CLAY_TEXT_ALIGN_LEFT
						}));
					}
				}
				CLAY(CLAY_ID("TopRight"), {
					.layout = {
						.sizing = { .width = CLAY_SIZING_PERCENT(0.5), .height = CLAY_SIZING_PERCENT(1.0) },
						.padding = CLAY_PADDING_ALL(16),
						.childGap = 16,
						.childAlignment = { CLAY_ALIGN_X_RIGHT, CLAY_ALIGN_Y_TOP },
						.layoutDirection = CLAY_LEFT_TO_RIGHT
					},
					.backgroundColor = (Clay_Color) { .r = 192, .g = 192, .b = 192, .a = 255 },
					.cornerRadius = CLAY_CORNER_RADIUS(4),
				}) {
					CLAY(CLAY_ID("BoxTR"), {
						.layout = {
							.sizing = { .width = CLAY_SIZING_PERCENT(0.2), .height = CLAY_SIZING_PERCENT(1.0) },
							.childAlignment = { CLAY_ALIGN_X_CENTER, CLAY_ALIGN_Y_CENTER }, // This alignment must be set to actually align the text
						},
						.backgroundColor = (Clay_Color) { .r = 156, .g = 156, .b = 156, .a = 255 },
						.cornerRadius = CLAY_CORNER_RADIUS(4),
					}) {
						Clay_OnHover(HandleInteraction, &swapped[1]); // Clicks should only be registered in the relevant UI element
						CLAY_TEXT(swapped[1] ? CLAY_STRING("Swapped") : CLAY_STRING("Click me!"), CLAY_TEXT_CONFIG({
							.textColor = (Clay_Color) { .r = 0, .g = 0, .b = 0, .a = 255 },
							.fontId = 0,
							.fontSize = 16,
							// .textAlignment = CLAY_TEXT_ALIGN_RIGHT
						}));
					}
				}
			}
		}

		Clay_RenderCommandArray render_commands = Clay_EndLayout();

		SDL_RenderClear(data.renderer);
		SDL_Clay_RenderClayCommands(&render_data, &render_commands);
		SDL_RenderPresent(data.renderer);
	}
}
