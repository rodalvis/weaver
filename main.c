#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <limits.h>
#include <string.h>

typedef struct
{
    float x;
    float y;
}
Point;

typedef struct
{
    Point a;
    Point b;
    Point c;
}
Tri;

typedef struct
{
    Tri* tri;
    int count;
    int max;
}
Tris;

typedef struct
{
    Point* point;
    int count;
    int max;
}
Points;

const Point zer = { 0.0f, 0.0f };

const Point one = { 1.0f, 1.0f };

static Tris tsnew(const int max)
{
    const Tris ts = { (Tri*) malloc(sizeof(Tri) * max), 0, max };
    return ts;
}

static Tris tsadd(Tris tris, const Tri tri)
{
    if(tris.count == tris.max)
    {
        puts("size limitation reached");
        exit(1);
    }
    tris.tri[tris.count++] = tri;
    return tris;
}

static int peql(const Point a, const Point b)
{
    return a.x == b.x && a.y == b.y;
}

static int incircum(const Tri t, const Point p)
{
    const float ax = t.a.x - p.x;
    const float ay = t.a.y - p.y;
    const float bx = t.b.x - p.x;
    const float by = t.b.y - p.y;
    const float cx = t.c.x - p.x;
    const float cy = t.c.y - p.y;
    const float det =
        (ax * ax + ay * ay) * (bx * cy - cx * by) -
        (bx * bx + by * by) * (ax * cy - cx * ay) +
        (cx * cx + cy * cy) * (ax * by - bx * ay);
    return det > 0.0f;
}

// Collects all edges from given triangles.
static Tris ecollect(Tris edges, const Tris in)
{
    for(int i = 0; i < in.count; i++)
    {
        const Tri tri = in.tri[i];
        const Tri ab = { tri.a, tri.b, zer };
        const Tri bc = { tri.b, tri.c, zer };
        const Tri ca = { tri.c, tri.a, zer };
        edges = tsadd(edges, ab);
        edges = tsadd(edges, bc);
        edges = tsadd(edges, ca);
    }
    return edges;
}

// Returns true if edge ab of two triangles are alligned.
static int alligned(const Tri a, const Tri b)
{
    return (peql(a.a, b.a) && peql(a.b, b.b)) || (peql(a.a, b.b) && peql(a.b, b.a));
}

// Flags alligned edges.
static Tris emark(Tris edges)
{
    for(int i = 0; i < edges.count; i++)
    {
        const Tri edge = edges.tri[i];
        for(int j = 0; j < edges.count; j++)
        {
            if(i == j)
                continue;
            const Tri other = edges.tri[j];
            if(alligned(edge, other))
                edges.tri[j].c = one;
        }
    }
    return edges;
}

// Creates new triangles from unique edges and appends to tris.
static Tris ejoin(Tris tris, const Tris edges, const Point p)
{
    for(int j = 0; j < edges.count; j++)
    {
        const Tri edge = edges.tri[j];
        if(peql(edge.c, zer))
        {
            const Tri tri = { edge.a, edge.b, p };
            tris = tsadd(tris, tri);
        }
    }
    return tris;
}

static SDL_Surface* load(const char* const path)
{
    SDL_Surface* const img = IMG_Load(path);
    if(!img)
    {
        puts(SDL_GetError());
        exit(1);
    }
    SDL_PixelFormat* const allocation = SDL_AllocFormat(SDL_PIXELFORMAT_ABGR8888);
    SDL_Surface* const converted = SDL_ConvertSurface(img, allocation, 0);
    return converted;
}

// Convolution - requires a 3x3 kernel.
static uint32_t conv(uint32_t* p, const int x, const int y, const int w, const int s, const int k[3][3])
{
    return
        (k[0][0] * (0xFF & (p[(x - 1) + (y - 1) * w] >> s))) +
        (k[0][1] * (0xFF & (p[(x - 0) + (y - 1) * w] >> s))) +
        (k[0][2] * (0xFF & (p[(x + 1) + (y - 1) * w] >> s))) +
        (k[1][0] * (0xFF & (p[(x - 1) + (y - 0) * w] >> s))) +
        (k[1][1] * (0xFF & (p[(x - 0) + (y - 0) * w] >> s))) +
        (k[1][2] * (0xFF & (p[(x + 1) + (y - 0) * w] >> s))) +
        (k[2][0] * (0xFF & (p[(x - 1) + (y + 1) * w] >> s))) +
        (k[2][1] * (0xFF & (p[(x - 0) + (y + 1) * w] >> s))) +
        (k[2][2] * (0xFF & (p[(x + 1) + (y + 1) * w] >> s)));
}

static int weight(const int k[3][3])
{
    return
        k[0][0] + k[0][1] + k[0][2] +
        k[1][0] + k[1][1] + k[1][2] +
        k[2][0] + k[2][1] + k[2][2];
}

static uint32_t* blur(uint32_t* const in, const int w, const int h)
{
    const int k[3][3] = {
        { 1, 2, 1 },
        { 2, 4, 2 },
        { 1, 2, 1 },
    };
    const int bytes = sizeof(*in) * w * h;
    uint32_t* const out = (uint32_t*) memcpy(malloc(bytes), in, bytes);
    for(int x = 1; x < w - 1; x++)
    for(int y = 1; y < h - 1; y++)
    {
        const uint32_t b = conv(in, x, y, w, 0x10, k) / weight(k);
        const uint32_t g = conv(in, x, y, w, 0x08, k) / weight(k);
        const uint32_t r = conv(in, x, y, w, 0x00, k) / weight(k);
        out[x + y * w] = (b << 0x10) | (g << 0x08) | (r << 0x00);
    }
    return out;
}

static uint32_t* grey(uint32_t* const in, const int w, const int h)
{
    const int bytes = sizeof(*in) * w * h;
    uint32_t* const out = (uint32_t*) memcpy(malloc(bytes), in, bytes);
    for(int x = 1; x < w - 1; x++)
    for(int y = 1; y < h - 1; y++)
    {
        const uint32_t lb = 0.21 * (0xFF & (in[x + y * w] >> 0x10));
        const uint32_t lg = 0.72 * (0xFF & (in[x + y * w] >> 0x08));
        const uint32_t lr = 0.07 * (0xFF & (in[x + y * w] >> 0x00));
        const uint32_t lum = lb + lg + lr;
        out[x + y * w] = (lum << 0x10) | (lum << 0x08) | (lum << 0x00);
    }
    return out;
}

static uint32_t max(uint32_t* out, const int w, const int h)
{
    uint32_t max = 0;
    for(int x = 1; x < w - 1; x++)
    for(int y = 1; y < h - 1; y++)
        if(out[x + y * w] > max)
            max = out[x + y * w];
    return max;
}

static void normalize(uint32_t* const in, const int w, const int h)
{
    const uint32_t mx = max(in, w, h);
    for(int x = 1; x < w - 1; x++)
    for(int y = 1; y < h - 1; y++)
        in[x + y * w] = 255 * in[x + y * w] / (float) mx;
}

static uint32_t* sobl(uint32_t* const in, const int w, const int h)
{
    const int kx[3][3] = {
        { -1, 0, 1 },
        { -2, 0, 2 },
        { -1, 0, 1 },
    };
    const int ky[3][3] = {
        {  1,  2,  1 },
        {  0,  0,  0 },
        { -1, -2, -1 },
    };
    const int bytes = sizeof(*in) * w * h;
    uint32_t* const out = (uint32_t*) memcpy(malloc(bytes), in, bytes);
    for(int x = 1; x < w - 1; x++)
    for(int y = 1; y < h - 1; y++)
    {
        const int vx = conv(in, x, y, w, 0x00, kx);
        const int vy = conv(in, x, y, w, 0x00, ky);
        out[x + y * w] = (uint32_t) sqrtf(vx * vx + vy * vy);
    }
    normalize(out, w, h);
    return out;
}

static int outob(const int x, const int y, const int w, const int h)
{
    return x < 0 || y < 0 || x >= w || y >= h;
}

static void draw(SDL_Renderer* const renderer, const int w, const int h, const Tris tris, uint32_t* regular)
{
    SDL_SetRenderDrawColor(renderer, 0x0, 0x0, 0x0, 0x0);
    SDL_RenderClear(renderer);
    for(int i = 0; i < tris.count; i++)
    {
        const Tri t = tris.tri[i];
        const int x = (t.a.x + t.b.x + t.c.x) / 3.0f;
        const int y = (t.a.y + t.b.y + t.c.y) / 3.0f;
        const uint32_t color = outob(x, y, w, h) ? 0x00 : regular[x + y * w];
        const uint32_t r = (color >> 0x00) & 0xFF;
        const uint32_t g = (color >> 0x08) & 0xFF;
        const uint32_t b = (color >> 0x10) & 0xFF;
        const uint32_t a = 0xFF;
        SDL_SetRenderDrawColor(renderer, r, g, b, a);
        const SDL_Point points[] = {
            { (int) t.a.x, (int) t.a.y },
            { (int) t.b.x, (int) t.b.y },
            { (int) t.c.x, (int) t.c.y },
            { (int) t.a.x, (int) t.a.y },
        };
        SDL_RenderDrawLines(renderer, points, sizeof(points) / sizeof(*points));
    }
    SDL_RenderPresent(renderer);
}

static void deltri(SDL_Renderer* const renderer, const Points ps, const int w, const int h, uint32_t* regular)
{
    const int size = w * h;
    Tris in = tsnew(size);
    Tris out = tsnew(size);
    Tris tris = tsnew(size);
    Tris edges = tsnew(size);
    // The super triangle will snuggley fit over the screen.
    const Tri super = { { (float) -w, 0.0f }, { 2.0f * w, 0.0f }, { w / 2.0f, 2.0f * h } };
    tris = tsadd(tris, super);
    for(int j = 0; j < ps.count; j++)
    {
        SDL_Event event;
        SDL_PollEvent(&event);
        if(event.type == SDL_QUIT || event.key.keysym.sym == SDLK_ESCAPE)
            break;
        in.count = out.count = edges.count = 0;
        const Point p = ps.point[j];
        // For all triangles...
        for(int i = 0; i < tris.count; i++)
        {
            const Tri tri = tris.tri[i];
            // Get triangles where point lies inside their circumcenter...
            if(incircum(tri, p))
                in = tsadd(in, tri);
            // And get triangles where point lies outside of their circumcenter.
            else out = tsadd(out, tri);
        }
        // Collect all triangle edges where point was inside circumcenter.
        edges = ecollect(edges, in);
        // Flag edges that are non-unique.
        edges = emark(edges);
        // Construct new triangles with unique edges.
        out = ejoin(out, edges, p);
        // Update triangle list.
        tris = out;
        // Loading bar.
        if(j % 100 == 0)
            draw(renderer, w, h, tris, regular);
    }
    // Flush.
    draw(renderer, w, h, tris, regular);
}

static Points psnew(const int max)
{
    const Points ps = { (Point*) malloc(sizeof(Point) * max), 0, max };
    return ps;
}

static Points pcollect(uint32_t* in, const int w, const int h, const uint32_t thresh)
{
    const int max = w * h;
    Points ps = psnew(max);
    for(int y = 1; y < h - 1; y++)
    for(int x = 1; x < w - 1; x++)
        if(in[x + y * w] > thresh)
        {
            const Point p = {
                (float) x,
                (float) y,
            };
            ps.point[ps.count++] = p;
        }
    return ps;
}

int main(int argc, char* argv[])
{
    if(argc != 3)
    {
        puts("use: path/to/image threshold");
        return 1;
    }
    SDL_Surface* surface = load(argv[1]);
    const uint32_t thresh = atoi(argv[2]);
    SDL_Window* window;
    SDL_Renderer* renderer;
    const int w = surface->w;
    const int h = surface->h;
    SDL_CreateWindowAndRenderer(w, h, 0, &window, &renderer);
    SDL_SetWindowTitle(window, "Weaver-1.3");
    // The image is first blurred, then grey scaled, then sobel filtered for edge detection.
    uint32_t* const a = (uint32_t*) surface->pixels;
    uint32_t* const b = blur(a, w, h);
    uint32_t* const c = grey(b, w, h);
    uint32_t* const d = sobl(c, w, h);
    // Collect all points - Higher thresholds yield fewer points.
    const Points ps = pcollect(d, w, h, thresh);
    // Note that the original image is used for coloring delaunay triangles.
    deltri(renderer, ps, w, h, a);
    puts("done");
    // Present and wait.
    SDL_Event event;
    do
    {
        SDL_PollEvent(&event);
        SDL_Delay(10);
    }
    while(event.type != SDL_KEYUP && event.type != SDL_QUIT);
    // No need to free hoisted memory - gives a fast exit.
    return 0;
}
