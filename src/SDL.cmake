set(BUILD_SHARED_LIBS_save BUILD_SHARED_LIBS)
set(BUILD_SHARED_LIBS 0)
set(SDL_SHARED_save SDL_SHARED)
set(SDL_SHARED 0)
set(SDL_STATIC_save SDL_STATIC)
set(SDL_STATIC 1)
set(SDL_ASSEMBLY_save SDL_ASSEMBLY)
set(SDL_ASSEMBLY 0)
get_directory_property(PROPERTIES EP_BASE EP_BASE_save)
set_directory_properties(PROPERTIES EP_BASE "ext/sdl3")
ExternalProject_Add(
    sdl3
    GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
    GIT_TAG main
)
set_directory_properties(PROPERTIES EP_BASE EP_BASE_save)
set(SDL_ASSEMBLY SDL_ASSEMBLY_save)
unset(SDL_ASSEMBLY_save)
set(SDL_STATIC SDL_STATIC_save)
unset(SDL_STATIC_save)
set(SDL_SHARED SDL_SHARED_save)
unset(SDL_SHARED_save)
set(BUILD_SHARED_LIBS BUILD_SHARED_LIBS_save)
unset(BUILD_SHARED_LIBS_save)
