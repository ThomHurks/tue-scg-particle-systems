// TinkerToy.cpp : Defines the entry point for the console application.
//

#include "BlockSparseMatrix.h"
#include "Particle.h"
#include "Force.h"
#include "GravityForce.h"
#include "Solver.h"
#include "SpringForce.h"
#include "RodConstraint.h"
#include "CircularWireConstraint.h"
#include "imageio.h"
#include "MouseSpringForce.h"
#include "AngularSpring.h"

#include <vector>
#include <stdlib.h>
#include <stdio.h>

#ifdef __APPLE__
#include <GLUT/glut.h>
#include <iostream>

#else
#include <GL/glut.h>
#endif

/* global variables */

static int N;
static double dt, d;
static int dsim;
static int dump_frames;
static int frame_number;

static std::vector<Particle*> pVector;
static std::vector<Force*> fVector;
static std::vector<Force*> cVector;
static double* * CVector;
static double* * CDotVector;
static SolverType m_SolverType;
static MouseSpringForce * msf = nullptr;
static bool userIsMouseInteracting = false;
static float * currentMousePosition = new float[2];

static int win_id;
static int win_x, win_y;
static int mouse_down[3];
static int mouse_release[3];
static int mouse_shiftclick[3];
static int omx, omy, mx, my;
static int hmx, hmy;

static RodConstraint * delete_this_dummy_rod = nullptr;
static CircularWireConstraint * delete_this_dummy_wire = nullptr;

/*
----------------------------------------------------------------------
free/clear/allocate simulation data
----------------------------------------------------------------------
 */

static void free_data(void)
{
    // Clear() also calls destructors of the objects inside the vectors.
    pVector.clear();
    fVector.clear();
    cVector.clear();

    delete delete_this_dummy_rod;
    delete_this_dummy_rod = nullptr;

    delete delete_this_dummy_wire;
    delete_this_dummy_wire = nullptr;

    delete msf;
    msf = nullptr;

    // Make sure arrays are deleted with correct delete[] syntax:
    delete[] CVector;
    CVector = nullptr;

    delete[] CDotVector;
    CDotVector = nullptr;

    delete[] currentMousePosition;
    currentMousePosition = nullptr;
}

static void clear_data(void)
{
    size_t ii, size = pVector.size();

    for (ii = 0; ii < size; ii++) {
        pVector[ii]->reset();
    }
}

static void initTest(void)
{
    const float dist = 0.2;
    const Vec2f center(0.0, 0.0);
    const Vec2f offset(dist, 0.0);
    const Vec2f offset2(dist, dist);
    const Vec2f offset3(0, dist);

    // Create three particles, attach them to each other, then add a
    // circular wire constraint to the first.

    m_SolverType = SolverType::Euler;
    int particleID = 0;
    pVector.push_back(new Particle(center + offset, particleID++));
    pVector.push_back(new Particle(center + offset + offset2, particleID++));
    pVector.push_back(new Particle(center + offset + offset + offset, particleID++));
    pVector.push_back(new Particle(center + offset + offset + offset + offset, particleID++));
    pVector[1]->m_Mass = 100.0;


    pVector.push_back(new Particle(center + offset3 + offset3, particleID++));
    pVector.push_back(new Particle(center + offset3 + offset3 + offset3, particleID++));
    pVector.push_back(new Particle(center + offset3 + offset3 + offset3 + offset, particleID++));

    // You should replace these with a vector generalized forces and one of
    // constraints...
    fVector.push_back(new GravityForce());
    fVector.push_back(new SpringForce(pVector[0], pVector[1], dist, 1.0, 1.0));
    fVector.push_back(new SpringForce(pVector[1], pVector[2], dist, 1.0, 1.0));
    fVector.push_back(new SpringForce(pVector[2], pVector[0], dist, 1.0, 1.0));
    fVector.push_back(new AngularSpring(pVector[4], pVector[5], pVector[6], PI / 3.0, 0.0001, 0));

    int constraintID = 0;
    BlockSparseMatrix J, JDot;
    int numConstraints = 1;
    CVector = new double*[numConstraints];
    CDotVector = new double*[numConstraints];
    cVector.push_back(new RodConstraint(pVector[2], pVector[3], dist, CVector, CDotVector, &J, &JDot , constraintID++));
    //delete_this_dummy_rod = new RodConstraint(pVector[2], pVector[3], dist);
    delete_this_dummy_wire = new CircularWireConstraint(pVector[0], center, dist);

    // The following code is purely to test the BlockSparseMatrix functionality and can be removed later:
    double x[] = {2};
    double r[] = {0, 0, 0, 0};
    J.matVecMult(x, r);
    int i, r_len = 4;
    for (i = 0; i < r_len; ++i) {
        std::cout << r[i];
    } // Should print out "0066" to console if everything works.
}

static void initCloth(void)
{
    const float dist = 0.1f;
    const Vec2f topLeft(-0.75f, 0.75f);
    const Vec2f offset(dist, 0.0);
    const Vec2f offset2(0.0, -dist);
    const int dim = 15;
    m_SolverType = SolverType::Euler;
    int particleID = 0;
    int i;
    int j;
    //init particles
    for (i = 0; i <= dim; i++) {
        for (j = 0; j <= dim; j++) {
            pVector.push_back(new Particle(topLeft + offset * i + offset2*j, particleID++));
        }
    }
    double ks = 0.01;
    double kd = 0.01;
    double rest = 0.075;
    for (i = 0; i < dim; i++) {
        for (j = 0; j < dim; j++) {
            int cur = j * (dim + 1) + i;
            int right = cur + dim + 1;
            int below = cur + 1;
            fVector.push_back(new SpringForce(pVector[cur], pVector[right], rest, ks, kd));
            fVector.push_back(new SpringForce(pVector[cur], pVector[below], rest, ks, kd));
        }
    }
    for (i = 0; i < dim; i++) {
        int cur1 = (i + 1)*(dim + 1) - 1;
        int right = cur1 + dim + 1;
        fVector.push_back(new SpringForce(pVector[cur1], pVector[right], rest, ks, kd));

        int cur2 = i + dim * (dim + 1);
        int below = cur2 + 1;
        fVector.push_back(new SpringForce(pVector[cur2], pVector[below], rest, ks, kd));

    }
}

/*
----------------------------------------------------------------------
OpenGL specific drawing routines
----------------------------------------------------------------------
 */

static void pre_display(void)
{
    glViewport(0, 0, win_x, win_y);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(-1.0, 1.0, -1.0, 1.0);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
}

static void post_display(void)
{
    // Write frames if necessary.
    if (dump_frames) {
        const int FRAME_INTERVAL = 4;
        if ((frame_number % FRAME_INTERVAL) == 0) {
            const int w = glutGet(GLUT_WINDOW_WIDTH);
            const int h = glutGet(GLUT_WINDOW_HEIGHT);
            if (w <= 0 || h <= 0)
            { exit(-1); }
            unsigned char * buffer = (unsigned char *) malloc(w * h * 4 * sizeof (unsigned char));
            if (!buffer)
            { exit(-1); }
            // glRasterPos2i(0, 0);
            glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
            static char filename[80];
            sprintf(filename, "snapshots/img%.5i.png", frame_number / FRAME_INTERVAL);
            printf("Dumped %s.\n", filename);
            saveImageRGBA(filename, buffer, w, h);

            free(buffer);
        }
    }
    frame_number++;

    glutSwapBuffers();
}

static void draw_particles(void)
{
    size_t ii, size = pVector.size();

    for (ii = 0; ii < size; ii++) {
        pVector[ii]->draw();
    }
}

static void draw_forces(void)
{
    size_t i, n = fVector.size();
    for (i = 0; i < n; ++i) {
        fVector[i]->draw();
    }
}

static void draw_constraints(void)
{
    size_t i, n = cVector.size();
    for (i = 0; i < n; ++i) {
        cVector[i]->draw();
    }
    // Delete this eventually:
    if (delete_this_dummy_rod)
        delete_this_dummy_rod->draw();
    if (delete_this_dummy_wire)
        delete_this_dummy_wire->draw();
}

/*
----------------------------------------------------------------------
relates mouse movements to tinker toy construction
----------------------------------------------------------------------
 */

static void get_from_UI()
{
    float i, j;
    // int size, flag;
    int hi, hj;
    // float x, y;
    if (!mouse_down[0] && !mouse_down[2] && !mouse_release[0]
            && !mouse_shiftclick[0] && !mouse_shiftclick[2]) return;

    i = ((mx / (float) win_x) * N);
    j = (((win_y - my) / (float) win_y) * N);

    if (i < 1 || i > N || j < 1 || j > N) return;


    hi = (int) ((hmx / (float) win_x) * N);
    hj = (int) (((win_y - hmy) / (float) win_y) * N);

    if (mouse_down[0])
    {
        currentMousePosition[0] = 2 * (i - N / 2) / N;
        currentMousePosition[1] = 2 * (j - N / 2) / N;
        if (!userIsMouseInteracting)
        {
            userIsMouseInteracting = true;
            if (msf == nullptr)
            {
                msf = new MouseSpringForce(pVector[0], 0, 0.4, 0, currentMousePosition);
                fVector.push_back(msf);
            }
        }
    }

    if (mouse_down[2]) {}

    if (mouse_release[0])
    {
        if (userIsMouseInteracting)
        {
            userIsMouseInteracting = false;
            if (fVector.size() > 0)
            { fVector.pop_back(); }
            delete msf;
            msf = nullptr;
            currentMousePosition[0] = 0;
            currentMousePosition[1] = 0;
        }
        mouse_release[0] = 0; //only need to record this once
    }

    omx = mx;
    omy = my;
}

static void remap_GUI()
{
    size_t ii, size = pVector.size();
    for (ii = 0; ii < size; ii++) {
        pVector[ii]->reset();
    }
}

/*
----------------------------------------------------------------------
GLUT callback routines
----------------------------------------------------------------------
 */

static void key_func(unsigned char key, int x, int y)
{
    switch (key) {
        case 'c':
        case 'C':
            clear_data();
            break;

        case 'd':
        case 'D':
            dump_frames = !dump_frames;
            break;

        case 'q':
        case 'Q':
            free_data();
            exit(0); // implicit break

        case ' ':
            dsim = !dsim;
            break;
        case '1':
            m_SolverType = SolverType::Euler;
            break;
        case '2':
            m_SolverType = SolverType::Midpoint;
            break;
        case '3':
            m_SolverType = SolverType::RungeKutta4;
            break;
        default:
            std::cout << "Invalid input!";
            break;
    }
}

static void mouse_func(int button, int state, int x, int y)
{
    omx = mx = x;
    omx = my = y;

    if (!mouse_down[0]) {
        hmx = x;
        hmy = y;
    }
    if (mouse_down[button]) mouse_release[button] = state == GLUT_UP;
    if (mouse_down[button]) mouse_shiftclick[button] = glutGetModifiers() == GLUT_ACTIVE_SHIFT;
    mouse_down[button] = state == GLUT_DOWN;
}

static void motion_func(int x, int y)
{
    mx = x;
    my = y;
}

static void reshape_func(int width, int height)
{
    glutSetWindow(win_id);
    glutReshapeWindow(width, height);

    win_x = width;
    win_y = height;
}

static void idle_func(void)
{
    if (dsim) {
        get_from_UI();
        simulation_step(pVector, fVector, cVector, dt, m_SolverType);
    } else {
        get_from_UI();
        remap_GUI();
    }

    glutSetWindow(win_id);
    glutPostRedisplay();
}

static void display_func(void)
{
    pre_display();

    draw_forces();
    draw_constraints();
    draw_particles();

    post_display();
}

/*
----------------------------------------------------------------------
open_glut_window --- open a glut compatible window and set callbacks
----------------------------------------------------------------------
 */

static void open_glut_window(void)
{
    glutInitDisplayMode(GLUT_RGBA | GLUT_DOUBLE);

    glutInitWindowPosition(0, 0);
    glutInitWindowSize(win_x, win_y);
    win_id = glutCreateWindow("Tinkertoys!");

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glutSwapBuffers();
    glClear(GL_COLOR_BUFFER_BIT);
    glutSwapBuffers();

    glEnable(GL_LINE_SMOOTH);
    glEnable(GL_POLYGON_SMOOTH);

    pre_display();

    glutKeyboardFunc(key_func);
    glutMouseFunc(mouse_func);
    glutMotionFunc(motion_func);
    glutReshapeFunc(reshape_func);
    glutIdleFunc(idle_func);
    glutDisplayFunc(display_func);
}

/*
----------------------------------------------------------------------
main --- main routine
----------------------------------------------------------------------
 */

int main(int argc, char ** argv)
{
    glutInit(&argc, argv);

    if (argc == 1) {
        N = 64;
        dt = 0.1f;
        d = 5.f;
        fprintf(stderr, "Using defaults : N=%d dt=%g d=%g\n",
                N, dt, d);
    } else {
        N = atoi(argv[1]);
        dt = atof(argv[2]);
        d = atof(argv[3]);
    }

    printf("\n\nHow to use this application:\n\n");
    printf("\t Toggle construction/simulation display with the spacebar key\n");
    printf("\t Dump frames by pressing the 'd' key\n");
    printf("\t Quit by pressing the 'q' key\n");

    dsim = 0;
    dump_frames = 0;
    frame_number = 0;

    // TODO: Make cloth and test interchangeable at runtime.
    //initCloth();
    initTest();

    win_x = 512;
    win_y = 512;
    open_glut_window();

    glutMainLoop();

    exit(0);
}

