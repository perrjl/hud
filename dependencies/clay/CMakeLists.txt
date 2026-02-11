project(clay
  LANGUAGES C
)

add_library(${PROJECT_NAME} STATIC clay_renderer_SDL3.h clay.h)
