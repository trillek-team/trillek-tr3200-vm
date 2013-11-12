#include "config_main.hpp"

#include <vm.hpp>

#include <iostream>
#include <fstream>
#include <ios>
#include <iomanip> 
#include <cstdio>
#include <algorithm>
#include <memory>
#include <string>
#include <cwctype>
#include <clocale>

#include <chrono>

#ifdef SDL2_ENABLE

unsigned sdl_width = 800;
unsigned sdl_height = 600;
int sdl_other_flags = SDL_WINDOW_SHOWN;
SDL_Window* pwin = nullptr;
SDL_Renderer* prend = nullptr;
SDL_GLContext ogl_context;

GLuint screenVBO; // ID of screen VBO

// Handler of shader program
GLuint shaderProgram;
 
// Ptrs to source doe of shaders
GLchar *vertexSource = nullptr;
GLchar *fragmentSource = nullptr;

// Handlers of the shader programs
GLuint vertexShader, fragmentShader;

const unsigned int shaderAttribute = 0;
 
static const GLfloat N_VERTICES=4;
float vdata[] = {
            1.0,  0.5, 0.0, // Top Right
           -1.0,  0.5, 0.0, // Top Left
            1.0, -1.0, 0.0, // Botton Right
           -1.0, -1.0, 0.0, // Bottom Left
};

void initSDL();
void initGL();
#endif



void print_regs(const vm::cpu::CpuState& state);
void print_pc(const vm::cpu::CpuState& state, const vm::ram::Mem& ram);
void print_stack(const vm::cpu::CpuState& state, const vm::ram::Mem& ram);

int main(int argc, char* argv[]) {
    using namespace vm;
    using namespace vm::cpu;

    byte_t* rom = NULL;
    size_t rom_size = 0;

    if (argc < 2) {
        std::printf("Usage: %s binary_file\n", argv[0]);
        return -1;

    } else {
        rom = new byte_t[64*1024];

        std::printf("Opening file %s\n", argv[1]);
        std::fstream f(argv[1], std::ios::in | std::ios::binary);
        unsigned count = 0;
#if (BYTE_ORDER != LITTLE_ENDIAN)
        while (f.good() && count < 64*1024) {
                f.read(reinterpret_cast<char*>(rom + count++), 1); // Read byte at byte, so the file must be in Little Endian
        }
#else
        size_t size;
        auto begin = f.tellg();
        f.seekg (0, std::ios::end);
        auto end = f.tellg();
        f.seekg (0, std::ios::beg);

        size = end - begin;
        size = size > (64*1024) ? (64*1024) : size;
        
        f.read(reinterpret_cast<char*>(rom), size);
        count = size;
#endif
        std::printf("Read %u bytes and stored in ROM\n", count);
        rom_size = count;
    }

    // Create the Virtual Machine
    VirtualComputer vm;
    vm.WriteROM(rom, rom_size);
    delete[] rom;

    // Add devices to tue Virtual Machine
    cda::CDA gcard;
    vm.AddDevice(0, gcard);

    vm.Reset();
    
    std::printf("Size of CPU state : %zu bytes \n", sizeof(vm.CPUState()) );
    
std::cout << "Run program (r) or Step Mode (s) ?\n";
    char mode;
    std::cin >> mode;
    std::getchar();


    bool debug = false;
    if (mode == 's' || mode == 'S') {
        debug = true;
    }
 

    std::cout << "Running!\n";
    unsigned ticks = 2000;
    unsigned long ticks_count = 0;

#ifdef SDL2_ENABLE
    initSDL();

    initGL();
#endif

    using namespace std::chrono;
    auto clock = high_resolution_clock::now();
    double delta;

    int c = ' ';
    bool loop = true;
    while ( loop) {
            
#ifdef SDL2_ENABLE
        SDL_Event e;
        while (SDL_PollEvent(&e)){
            //If user closes he window
            if (e.type == SDL_QUIT)
                loop = false;
            //else if (e.type == SDL_KEYDOWN) {
            
            //}
        }
#endif

        if (debug) {
            print_pc(vm.CPUState(), vm.RAM());
            if (vm.CPUState().skiping)
                std::printf("Skiping!\n");
            if (vm.CPUState().sleeping)
                std::printf("ZZZZzzzz...\n");
        }

        if (!debug) {
            // cpu.Tick(ticks);
            vm.Tick(ticks);
            ticks_count += ticks;
            

            auto oldClock = clock;
            clock = high_resolution_clock::now();
            if (ticks <= 0) // Compensates if is too quick
                    delta += duration_cast<nanoseconds>(clock - oldClock).count();
            else
                    delta = duration_cast<nanoseconds>(clock - oldClock).count();

            ticks = delta/100.0f + 0.5f; // Rounding bug correction
        } else
            ticks = vm.Step(); // cpu.Step();


        // Speed info
        if (!debug && ticks_count > 5000000) {
            std::cout << "Running " << ticks << " cycles in " << delta << " nS"
                                << " Speed of " 
                                << 100.0f * (((ticks * 1000000000.0) / vm.Clock()) / delta)
                                << "% \n";
            std::cout << std::endl;
            ticks_count -= 5000000;
        }


        if (debug) {
            std::printf("Takes %u cycles\n", vm.CPUState().wait_cycles);
            print_regs(vm.CPUState());
            print_stack(vm.CPUState(), vm.RAM());
            c = std::getchar();
            if (c == 'q' || c == 'Q')
                loop = false;

        }

#ifdef SDL2_ENABLE
        // Clear The Screen And The Depth Buffer
        glClearColor( 0.1f, 0.1f, 0.1f, 1.0f );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Drawing ... 
        glUseProgram(shaderProgram);
        // Enable attribute index 0(shaderAttribute) as being used
        glEnableVertexAttribArray(shaderAttribute);
        glBindBuffer(GL_ARRAY_BUFFER, screenVBO);
        // Vertex data to attribute index 0 (shadderAttribute) and is 3 floats
        glVertexAttribPointer(
            shaderAttribute, 
            3, 
            GL_FLOAT, 
            GL_FALSE, 
            0, 
            0 );

        glDrawArrays(GL_TRIANGLE_STRIP, shaderAttribute, N_VERTICES);

        glDisableVertexAttribArray(shaderAttribute);

        // Update host window
        SDL_RenderPresent(prend);
        SDL_GL_SwapWindow(pwin);
#endif
    }

#ifdef SDL2_ENABLE

    SDL_GL_DeleteContext(ogl_context);
    SDL_DestroyWindow(pwin);
    SDL_Quit();
#endif

    return 0;
}


void print_regs(const vm::cpu::CpuState& state) {
    // Print registers
    for (int i=0; i < 27; i++) {
        std::printf("%%r%2d= 0x%08X ", i, state.r[i]);
        if (i == 3 || i == 7 || i == 11 || i == 15 || i == 19 || i == 23 || i == 27 || i == 31)
        std::printf("\n");
    }
    std::printf("%%y= 0x%08X\n", Y);
    
    std::printf("%%ia= 0x%08X ", IA);
    std::printf("%%flags= 0x%08X ", FLAGS);
    std::printf("%%bp= 0x%08X ", state.r[BP]);
    std::printf("%%sp= 0x%08X\n", state.r[SP]);

    std::printf("%%pc= 0x%08X \n", state.pc);
    std::printf("EDE: %d EOE: %d ESS: %d EI: %d \t IF: %d DE %d OF: %d CF: %d\n",
                    GET_EDE(FLAGS), GET_EOE(FLAGS), GET_ESS(FLAGS), GET_EI(FLAGS),
                    GET_IF(FLAGS) , GET_DE(FLAGS) , GET_OF(FLAGS) , GET_CF(FLAGS));
    std::printf("\n");

}

void print_pc(const vm::cpu::CpuState& state, const vm::ram::Mem&  ram) {
    vm::dword_t val = ram.RD(state.pc);
    
    std::printf("\tPC : 0x%08X > 0x%08X ", state.pc, val); 
    std::cout << vm::cpu::Disassembly(ram,  state.pc) << std::endl;  
}

void print_stack(const vm::cpu::CpuState& state, const vm::ram::Mem& ram) {
    std::printf("STACK:\n");

    for (size_t i =0; i <5*4; i +=4) {
            auto val = ram.RD(state.r[SP]+ i);

            std::printf("0x%08X\n", val);
            if (((size_t)(state.r[SP]) + i) >= 0xFFFFFFFF)
                    break;
    }
}


#ifdef SDL2_ENABLE

void initSDL() {
    // Init SDL2 / OpenGL stuff
    if (SDL_Init(SDL_INIT_VIDEO) == -1){
        std::cout << SDL_GetError() << std::endl;
        exit(-1);
    }
    
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION,3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION,1);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_RendererInfo rend_info;

    pwin = SDL_CreateWindow("RC3200 VM", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
              sdl_width, sdl_height, SDL_WINDOW_OPENGL | sdl_other_flags);

    ogl_context = SDL_GL_CreateContext(pwin);

    GLenum status = glewInit();
    if (status != GLEW_OK) {
        std::cerr << "GLEW Error: " << glewGetErrorString(status) << "\n";
        exit(1);
    }
    
    // sync buffer swap with monitor's vertical refresh rate
    SDL_GL_SetSwapInterval(1);
}

void initGL() {
    // Init OpenGL ************************************************************
    
    // Initialize VBO *********************************************************
    glGenBuffers(1, &screenVBO);
    glBindBuffer(GL_ARRAY_BUFFER, screenVBO);
    // Upload data to VBO
    glBufferData(GL_ARRAY_BUFFER, N_VERTICES * 3 * sizeof(GLfloat), vdata, GL_STATIC_DRAW);
    
    
    // Loading shaders ********************************************************
    auto f_vs = std::fopen("./assets/shaders/mvp_template.vert", "r");
    if (f_vs != nullptr) {
      fseek(f_vs, 0L, SEEK_END);
      size_t bufsize = ftell(f_vs);
      vertexSource = new GLchar[bufsize + 1]();

      fseek(f_vs, 0L, SEEK_SET);
      fread(vertexSource, sizeof(GLchar), bufsize, f_vs);
      
      fclose(f_vs);
      vertexSource[bufsize] = 0; // Enforce null char
    }

    auto f_fs = std::fopen("./assets/shaders/basic_fs.frag", "r");
    if (f_fs != nullptr) {
      fseek(f_fs, 0L, SEEK_END);
      size_t bufsize = ftell(f_fs);
      fragmentSource = new GLchar[bufsize +1 ]();

      fseek(f_fs, 0L, SEEK_SET);
      fread(fragmentSource, sizeof(GLchar), bufsize, f_fs);
      
      fclose(f_fs);
      fragmentSource[bufsize] = 0; // Enforce null char
    }


    // Assign our handles a "name" to new shader objects
    vertexShader = glCreateShader(GL_VERTEX_SHADER);
    fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
 
    // Associate the source code buffers with each handle
    glShaderSource(vertexShader, 1, (const GLchar**)&vertexSource, 0);
    glShaderSource(fragmentShader, 1, (const GLchar**)&fragmentSource, 0);

    // Compile our shader objects
    glCompileShader(vertexShader);
    glCompileShader(fragmentShader);

    if (vertexSource != nullptr)
      delete[] vertexSource;

    if (fragmentSource != nullptr)
      delete[] fragmentSource;
   
    
    GLint Result = GL_FALSE;
    int InfoLogLength;
    
    // Vertex Shader compiling error messages
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &Result);
    glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &InfoLogLength);
    std::vector<char> VertexShaderErrorMessage(InfoLogLength); 
    glGetShaderInfoLog(vertexShader, InfoLogLength, NULL, &VertexShaderErrorMessage[0]);
    std::printf("%s\n", &VertexShaderErrorMessage[0]);
    
    // Fragment Shader compiling error messages
    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &Result);
    glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &InfoLogLength);
    std::vector<char> FragmentShaderErrorMessage(InfoLogLength); 
    glGetShaderInfoLog(fragmentShader, InfoLogLength, NULL, &FragmentShaderErrorMessage[0]);
    std::printf("%s\n", &FragmentShaderErrorMessage[0]);
    

    // Assign our program handle a "name"
    shaderProgram = glCreateProgram();
 
    // Attach our shaders to our program
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);

    // Bind attribute index 0 (shaderAttribute) to in_Position
    glBindAttribLocation(shaderProgram, shaderAttribute, "in_Position");
 
    // Link shader program
    glLinkProgram(shaderProgram);
     
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
}

#endif

