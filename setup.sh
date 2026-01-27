#!/usr/bin/env bash
if [ ! -d "./dependecies/SDL_ttf/external/freetype" ]; then
	cd ./dependencies/SDL_ttf/external
	./download.sh
	cd ../../..
fi
