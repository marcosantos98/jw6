mkdir -f web | out-null
em++ -o ./web/index.html main.cpp -Os -Wall ./raylib/libraylib.a -I./arena -I./raylib/include -L./raylib -s USE_GLFW=3 -DPLATFORM_WEB --shell-file ./raylib/minshell.html --preload-file=./res/
