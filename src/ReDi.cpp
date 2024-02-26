#include <iostream>
#include <fstream>
#include <SDL2/SDL.h>
#include "Utils.hpp"
#include "../include/json.hpp"
#include "../include/argparse.hpp"

using namespace std;

// Parameters
int WIDTH, HEIGHT, INIT_SHAPE, INIT_SIZE, FRAMES_TO_RENDER;
float FEED, KILL;
const float weights[8] = {0.05, 0.2, 0.05, 0.2, 0.2, 0.05, 0.2, 0.05}, D_A = 1.0, D_B = 0.5;

// Stats
int frame = 0, fps = 0, fps_counter = 0, cells_drawn = 0;
map<string, int *> stats{{"1. FPS: ", &fps}, {"2. Frame: ", &frame}, {"3. Cells Drawn: ", &cells_drawn}};
Uint32 fps_timer, draw_timer;

// SDL Stuff
SDL_Renderer *renderer;
SDL_Window *win;
SDL_Texture *texture;
SDL_Event event;

SDL_PixelFormat *format;
Uint32 *pixels;
SDL_Color color;
int pitch;

class Cell
{
private:
    float a = 1, b = 0, lap_a = 0, lap_b = 0;
    Uint8 last_c = 255;
    Uint32 pos;
    Cell *neighbours[8];

public:
    Cell() = default;
    Cell(int row_, int column_)
    {
        pos = row_ * WIDTH + column_;
    }
    void seed_b()
    {
        b = 1;
    }
    void assign_neighbours(Cell **cells)
    {
        short int index = 0;
        const int row = pos / WIDTH, column = pos % WIDTH;
        for (int r = row - 1; r < row + 2; r++)
        {
            for (int c = column - 1; c < column + 2; c++)
            {
                if (!(row == r && column == c))
                {
                    neighbours[index] = &cells[r][c];
                    ++index;
                }
            }
        }
    }
    void calculate_laplacian()
    {
        lap_a = -a;
        lap_b = -b;
        for (int i = 0; i < 8; ++i)
        {
            lap_a += (neighbours[i]->a * weights[i]);
            lap_b += (neighbours[i]->b * weights[i]);
        }
    }
    void update()
    {
        a += (D_A * lap_a - a * b * b + FEED * (1 - a));
        b += (D_B * lap_b + a * b * b - (FEED + KILL) * b);
    }
    void draw()
    {
        const Uint8 c = Clamp0255((a - b) * 255);
        if (last_c != c)
        {
            pixels[pos] = SDL_MapRGB(format, Clamp0255(color.r - c), Clamp0255(color.g - c), Clamp0255(color.b - c));
            last_c = c;
            ++cells_drawn;
        }
    }
};

Cell **cells;

void update()
{
    for (int row = 1; row < HEIGHT - 1; row++)
    {
        for (int column = 1; column < WIDTH - 1; column++)
        {
            cells[row][column].calculate_laplacian();
        }
    }
    for (int row = 1; row < HEIGHT - 1; row++)
    {
        for (int column = 1; column < WIDTH - 1; column++)
        {
            cells[row][column].update();
        }
    }

    frame++;
    fps_counter++;

    if (frame == FRAMES_TO_RENDER)
        exit(0);

    // Calculating FPS
    if (SDL_GetTicks() - fps_timer > 1000.f)
    {
        fps = fps_counter;
        fps_counter = 0;
        fps_timer = SDL_GetTicks();
    }
    print_stats(&stats);
}

void draw_screen()
{
    cells_drawn = 0;
    SDL_LockTexture(texture, NULL, (void **)&pixels, &pitch);
    for (int row = 0; row < HEIGHT; row++)
    {
        for (int column = 0; column < WIDTH; column++)
        {
            cells[row][column].draw();
        }
    }
    SDL_UnlockTexture(texture);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

void handle_events()
{
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_QUIT)
            exit(0);
    }
}

void init_cells()
{
    cells = new Cell *[HEIGHT];

    // Initialise rows
    for (int row = 0; row < HEIGHT; row++)
    {
        cells[row] = new Cell[WIDTH];
    }

    // Initialise Cells
    for (int row = 0; row < HEIGHT; row++)
    {
        for (int column = 0; column < WIDTH; column++)
        {
            cells[row][column] = Cell(row, column);
        }
    }

    // Assigning neighbours to cells
    for (int row = 1; row < HEIGHT - 1; row++)
    {
        for (int column = 1; column < WIDTH - 1; column++)
        {
            cells[row][column].assign_neighbours(cells);
        }
    }
    int c_column = floor(WIDTH / 2);
    int c_row = floor(HEIGHT / 2);

    // Seeding the init shape with b
    for (int row = c_row - INIT_SIZE; row < c_row + INIT_SIZE; row++)
    {
        for (int column = c_column - INIT_SIZE; column < c_column + INIT_SIZE; column++)
        {
            if (INIT_SHAPE == CIRCLE)
            {
                if (is_point_in_circle(column, row, c_column, c_row, INIT_SIZE))
                    cells[row][column].seed_b();
            }
            else if (INIT_SHAPE == SQUARE)
                cells[row][column].seed_b();
        }
    }
}

void InitSDL()
{
    SDL_Init(SDL_INIT_EVERYTHING);
    win = SDL_CreateWindow("Reaction Diffusion", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, WIDTH, HEIGHT, 0);
    renderer = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGR888, SDL_TEXTUREACCESS_STREAMING, WIDTH, HEIGHT);
    format = SDL_AllocFormat(SDL_PIXELFORMAT_BGR888);
}

void load_args(int argc, char *argv[])
{
    // Configuration
    nlohmann::json config;
    fstream config_file;

    config_file.open("../config.json", ios::in);
    config_file >> config;
    config_file.close();

    WIDTH = config["width"];
    HEIGHT = config["height"];
    FRAMES_TO_RENDER = config["frames_to_render"];

    nlohmann::json system = config["systems"][(string)config["defaultSystem"]];
    FEED = system["feed"];
    KILL = system["kill"];
    INIT_SHAPE = system["init_shape"];
    INIT_SIZE = system["init_size"];

    color.r = config["color"].at(0);
    color.g = config["color"].at(1);
    color.b = config["color"].at(2);

    argparse::ArgumentParser parser("Reaction Diffusion Simulator");
    parser.add_description("A program to simulate Reaction Diffusion using the Gray-Scott model. Give initial parameters as arguments or edit the config.json file.");

    parser.add_argument("--width", "-w")
        .default_value(WIDTH)
        .scan<'i', int>()
        .help("Set the width of the simulation.");

    parser.add_argument("--height", "-ht")
        .default_value(HEIGHT)
        .scan<'i', int>()
        .help("Set the height of the simulation.");

    parser.add_argument("--frames_to_render", "-f")
        .default_value(FRAMES_TO_RENDER)
        .scan<'i', int>()
        .help("Set the number of frames to run the simulation.");

    parser.add_argument("--system", "-s")
        .help("Set the initial parameters according to a given system present in config.json.\n\
                Note that any parameters passed as arguments will override the system parameters.")

        .default_value((string)config["defaultSystem"]);

    parser.add_argument("--feed", "-f")
        .default_value(FEED)
        .scan<'g', float>()
        .help("Set the feed rate of the simulation.");

    parser.add_argument("--kill", "-k")
        .default_value(KILL)
        .scan<'g', float>()
        .help("Set the kill rate of the simulation.");

    parser.add_argument("--init_shape", "-ishape")
        .default_value(INIT_SHAPE)
        .scan<'i', int>()
        .help("Set the shape of the area initialised with chemical b.");

    parser.add_argument("--init_size", "-isize")
        .default_value(INIT_SIZE)
        .scan<'i', int>()
        .help("Set the size of the area initialised with chemical b.");

    try
    {
        parser.parse_args(argc, argv);
    }
    catch (const runtime_error &err)
    {
        cout << err.what() << endl;
        cout << parser;
        exit(0);
    }
    WIDTH = parser.get<int>("--width");
    HEIGHT = parser.get<int>("--height");
    FRAMES_TO_RENDER = parser.get<int>("--frames_to_render");

    if (!config["systems"].contains(parser.get<string>("--system")))
    {
        cout << "System is not included in config.json. Systems Present: " << endl;
        for (auto system : config["systems"].items())
        {
            cout << system.key() << endl;
        }
        exit(0);
    }

    system = config["systems"][parser.get<string>("--system")];
    FEED = system["feed"];
    KILL = system["kill"];
    INIT_SHAPE = system["init_shape"];
    INIT_SIZE = system["init_size"];

    if (parser.is_used("--feed"))
        FEED = parser.get<float>("--feed");
    if (parser.is_used("--kill"))
        KILL = parser.get<float>("--kill");
    if (parser.is_used("--init_shape"))
        INIT_SHAPE = parser.get<int>("--init_shape");
    if (parser.is_used("--init_size"))
        INIT_SIZE = parser.get<int>("--init_size");
    if (INIT_SHAPE != CIRCLE && INIT_SHAPE != SQUARE)
    {
        cout << "Init Shape must be either 0 or 1 i.e Circle or Square." << endl;
        exit(1);
    }
}

int main(int argc, char *argv[])
{
    load_args(argc, argv);
    init_cells();
    InitSDL();
    fps_timer = SDL_GetTicks();
    draw_timer = SDL_GetTicks();
    while (1)
    {
        handle_events();
        if (SDL_GetTicks() - draw_timer > (float)(1000 / 60))
        {
            draw_screen();
            draw_timer = SDL_GetTicks();
        }
        update();
    }
}
