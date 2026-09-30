#include "ThreeScrambler.h"
#include <vector>
#include <cstring>

using namespace std;

#define MIN_SCRAMBLE_MOVES 17
#define MAX_SCRAMBLE_MOVES 22
#define MAX_DIFF_MOVES 18
#define PERMITED_MOVES 15

static const string moves[MAX_DIFF_MOVES] = {
    "L", "R", "U", "D", "F", "B",
    "L'", "R'", "U'", "D'", "F'", "B'",
    "L2", "R2", "U2", "D2", "F2", "B2"
};

static int solvedCube[108] = {
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0,  //BLANCO 1 / VERDE 2 / ROJO 3 / AZUL 4 / NARANJA 5 / AMARILLO 6
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 0,
    5, 5, 5, 2, 2, 2, 3, 3, 3, 4, 4, 4,
    5, 5, 5, 2, 2, 2, 3, 3, 3, 4, 4, 4,
    5, 5, 5, 2, 2, 2, 3, 3, 3, 4, 4, 4,
    0, 0, 0, 6, 6, 6, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 6, 6, 6, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 6, 6, 6, 0, 0, 0, 0, 0, 0
};

static int visualCube[108];

static std::vector<std::string> tempMoves;
static int selection, numMoves;
static string lastMove;

static void getPossibleMoves();
static void procesarMovimiento(string mov);
static void rotarCaraHorario(int r, int c);
static void rotarCaraAntihorario(int r, int c);
static void rotarCara180(int r, int c);

string ThreeScrambler::scramble() {
    string scramble = "";
    numMoves = rand() % (MAX_SCRAMBLE_MOVES + 1 - MIN_SCRAMBLE_MOVES) + MIN_SCRAMBLE_MOVES;
    selection = rand() % MAX_DIFF_MOVES;
    scramble += moves[selection];
    lastMove = moves[selection];
    numMoves--;
    for (int i = 0; i < numMoves; i++) {
        getPossibleMoves();
        selection = rand() % PERMITED_MOVES;
        scramble += " " + tempMoves[selection];
        lastMove = tempMoves[selection];
    }

    return scramble;
}

static void getPossibleMoves() {
    if (lastMove == "L" || lastMove == "L'" || lastMove == "L2") {
        tempMoves = {
            "R", "U", "D", "F", "B",
            "R'", "U'", "D'", "F'", "B'",
            "R2", "U2", "D2", "F2", "B2"
        };
        return;
    }

    if (lastMove == "R" || lastMove == "R'" || lastMove == "R2") {
        tempMoves = {
            "L", "U", "D", "F", "B",
            "L'", "U'", "D'", "F'", "B'",
            "L2", "U2", "D2", "F2", "B2"
        };
        return;
    }

    if (lastMove == "U" || lastMove == "U'" || lastMove == "U2") {
        tempMoves = {
            "L", "R", "D", "F", "B",
            "L'", "R'", "D'", "F'", "B'",
            "L2", "R2", "D2", "F2", "B2"
        };
        return;
    }

    if (lastMove == "D" || lastMove == "D'" || lastMove == "D2") {
        tempMoves = {
            "L", "R", "U", "F", "B",
            "L'", "R'", "U'", "F'", "B'",
            "L2", "R2", "U2", "F2", "B2"
        };
        return;
    }

    if (lastMove == "F" || lastMove == "F'" || lastMove == "F2") {
        tempMoves = {
            "L", "R", "U", "D", "B",
            "L'", "R'", "U'", "D'", "B'",
            "L2", "R2", "U2", "D2", "B2"
        };
        return;
    }

    if (lastMove == "B" || lastMove == "B'" || lastMove == "B2") {
        tempMoves = {
            "L", "R", "U", "D", "F",
            "L'", "R'", "U'", "D'", "F'",
            "L2", "R2", "U2", "D2", "F2"
        };
        return;
    }
}

int* ThreeScrambler::visualize(string scramble) {
    memcpy(visualCube, solvedCube, sizeof(solvedCube));
    string mov = "";

    for (unsigned int i = 0; i <= scramble.length(); i++) {
        if (i < scramble.length() && scramble[i] != ' ') {
        mov += scramble[i];
        } else if (mov.length() > 0) {
            procesarMovimiento(mov);
            mov = "";
        }
    }

    return visualCube;
}

static void rotarCaraHorario(int r, int c) {
    int v[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            v[i][j] = visualCube[(r + i) * 12 + (c + j)];
        }
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            visualCube[(r + i) * 12 + (c + j)] = v[2 - j][i];
        }
    }
}

static void rotarCaraAntihorario(int r, int c) {
    int v[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            v[i][j] = visualCube[(r + i) * 12 + (c + j)];
        }
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            visualCube[(r + i) * 12 + (c + j)] = v[j][2 - i];
        }
    }
}

static void rotarCara180(int r, int c) {
    int v[3][3];
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            v[i][j] = visualCube[(r + i) * 12 + (c + j)];
        }
    }
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            visualCube[(r + i) * 12 + (c + j)] = v[2 - i][2 - j];
        }
    }
}

static void procesarMovimiento(string mov) {
    int visual[108];
    memcpy(visual, visualCube, sizeof(visualCube));

    if (mov == "L") {
        rotarCaraHorario(3, 0);

        visualCube[0 * 12 + 3] = visual[5 * 12 + 11];
        visualCube[1 * 12 + 3] = visual[4 * 12 + 11];
        visualCube[2 * 12 + 3] = visual[3 * 12 + 11];

        visualCube[3 * 12 + 3] = visual[0 * 12 + 3];
        visualCube[4 * 12 + 3] = visual[1 * 12 + 3];
        visualCube[5 * 12 + 3] = visual[2 * 12 + 3];

        visualCube[6 * 12 + 3] = visual[3 * 12 + 3];
        visualCube[7 * 12 + 3] = visual[4 * 12 + 3];
        visualCube[8 * 12 + 3] = visual[5 * 12 + 3];

        visualCube[5 * 12 + 11] = visual[6 * 12 + 3];
        visualCube[4 * 12 + 11] = visual[7 * 12 + 3];
        visualCube[3 * 12 + 11] = visual[8 * 12 + 3];
        return;
    }
    if (mov == "L'") {
        rotarCaraAntihorario(3, 0);

        visualCube[0 * 12 + 3] = visual[3 * 12 + 3];
        visualCube[1 * 12 + 3] = visual[4 * 12 + 3];
        visualCube[2 * 12 + 3] = visual[5 * 12 + 3];

        visualCube[3 * 12 + 3] = visual[6 * 12 + 3];
        visualCube[4 * 12 + 3] = visual[7 * 12 + 3];
        visualCube[5 * 12 + 3] = visual[8 * 12 + 3];

        visualCube[6 * 12 + 3] = visual[5 * 12 + 11];
        visualCube[7 * 12 + 3] = visual[4 * 12 + 11];
        visualCube[8 * 12 + 3] = visual[3 * 12 + 11];

        visualCube[5 * 12 + 11] = visual[0 * 12 + 3];
        visualCube[4 * 12 + 11] = visual[1 * 12 + 3];
        visualCube[3 * 12 + 11] = visual[2 * 12 + 3];
        return;
    }
    if (mov == "L2") {
        rotarCara180(3, 0);

        visualCube[0 * 12 + 3] = visual[6 * 12 + 3];
        visualCube[1 * 12 + 3] = visual[7 * 12 + 3];
        visualCube[2 * 12 + 3] = visual[8 * 12 + 3];

        visualCube[3 * 12 + 3] = visual[5 * 12 + 11];
        visualCube[4 * 12 + 3] = visual[4 * 12 + 11];
        visualCube[5 * 12 + 3] = visual[3 * 12 + 11];

        visualCube[6 * 12 + 3] = visual[0 * 12 + 3];
        visualCube[7 * 12 + 3] = visual[1 * 12 + 3];
        visualCube[8 * 12 + 3] = visual[2 * 12 + 3];

        visualCube[5 * 12 + 11] = visual[3 * 12 + 3];
        visualCube[4 * 12 + 11] = visual[4 * 12 + 3];
        visualCube[3 * 12 + 11] = visual[5 * 12 + 3];
        return;
    }
    if (mov == "R") {
        rotarCaraHorario(3, 6);

        visualCube[0 * 12 + 5] = visual[3 * 12 + 5];
        visualCube[1 * 12 + 5] = visual[4 * 12 + 5];
        visualCube[2 * 12 + 5] = visual[5 * 12 + 5];

        visualCube[3 * 12 + 5] = visual[6 * 12 + 5];
        visualCube[4 * 12 + 5] = visual[7 * 12 + 5];
        visualCube[5 * 12 + 5] = visual[8 * 12 + 5];

        visualCube[6 * 12 + 5] = visual[5 * 12 + 9];
        visualCube[7 * 12 + 5] = visual[4 * 12 + 9];
        visualCube[8 * 12 + 5] = visual[3 * 12 + 9];

        visualCube[5 * 12 + 9] = visual[0 * 12 + 5];
        visualCube[4 * 12 + 9] = visual[1 * 12 + 5];
        visualCube[3 * 12 + 9] = visual[2 * 12 + 5];
        return;
    }
    if (mov == "R'") {
        rotarCaraAntihorario(3, 6);

        visualCube[0 * 12 + 5] = visual[5 * 12 + 9];
        visualCube[1 * 12 + 5] = visual[4 * 12 + 9];
        visualCube[2 * 12 + 5] = visual[3 * 12 + 9];

        visualCube[3 * 12 + 5] = visual[0 * 12 + 5];
        visualCube[4 * 12 + 5] = visual[1 * 12 + 5];
        visualCube[5 * 12 + 5] = visual[2 * 12 + 5];

        visualCube[6 * 12 + 5] = visual[3 * 12 + 5];
        visualCube[7 * 12 + 5] = visual[4 * 12 + 5];
        visualCube[8 * 12 + 5] = visual[5 * 12 + 5];

        visualCube[5 * 12 + 9] = visual[6 * 12 + 5];
        visualCube[4 * 12 + 9] = visual[7 * 12 + 5];
        visualCube[3 * 12 + 9] = visual[8 * 12 + 5];
        return;
    }
    if (mov == "R2") {
        rotarCara180(3, 6);

        visualCube[0 * 12 + 5] = visual[6 * 12 + 5];
        visualCube[1 * 12 + 5] = visual[7 * 12 + 5];
        visualCube[2 * 12 + 5] = visual[8 * 12 + 5];

        visualCube[3 * 12 + 5] = visual[5 * 12 + 9];
        visualCube[4 * 12 + 5] = visual[4 * 12 + 9];
        visualCube[5 * 12 + 5] = visual[3 * 12 + 9];

        visualCube[6 * 12 + 5] = visual[0 * 12 + 5];
        visualCube[7 * 12 + 5] = visual[1 * 12 + 5];
        visualCube[8 * 12 + 5] = visual[2 * 12 + 5];

        visualCube[5 * 12 + 9] = visual[3 * 12 + 5];
        visualCube[4 * 12 + 9] = visual[4 * 12 + 5];
        visualCube[3 * 12 + 9] = visual[5 * 12 + 5];
        return;
    }
    if (mov == "F") {
        rotarCaraHorario(3, 3);

        visualCube[2 * 12 + 3] = visual[5 * 12 + 2];
        visualCube[2 * 12 + 4] = visual[4 * 12 + 2];
        visualCube[2 * 12 + 5] = visual[3 * 12 + 2];

        visualCube[3 * 12 + 6] = visual[2 * 12 + 3];
        visualCube[4 * 12 + 6] = visual[2 * 12 + 4];
        visualCube[5 * 12 + 6] = visual[2 * 12 + 5];

        visualCube[6 * 12 + 5] = visual[3 * 12 + 6];
        visualCube[6 * 12 + 4] = visual[4 * 12 + 6];
        visualCube[6 * 12 + 3] = visual[5 * 12 + 6];

        visualCube[3 * 12 + 2] = visual[6 * 12 + 3];
        visualCube[4 * 12 + 2] = visual[6 * 12 + 4];
        visualCube[5 * 12 + 2] = visual[6 * 12 + 5];
        return;
    }
    if (mov == "F'") {
        rotarCaraAntihorario(3, 3);

        visualCube[2 * 12 + 3] = visual[3 * 12 + 6];
        visualCube[2 * 12 + 4] = visual[4 * 12 + 6];
        visualCube[2 * 12 + 5] = visual[5 * 12 + 6];

        visualCube[3 * 12 + 6] = visual[6 * 12 + 5];
        visualCube[4 * 12 + 6] = visual[6 * 12 + 4];
        visualCube[5 * 12 + 6] = visual[6 * 12 + 3];

        visualCube[6 * 12 + 3] = visual[3 * 12 + 2];
        visualCube[6 * 12 + 4] = visual[4 * 12 + 2];
        visualCube[6 * 12 + 5] = visual[5 * 12 + 2];

        visualCube[5 * 12 + 2] = visual[2 * 12 + 3];
        visualCube[4 * 12 + 2] = visual[2 * 12 + 4];
        visualCube[3 * 12 + 2] = visual[2 * 12 + 5];
        return;
    }
    if (mov == "F2") {
        rotarCara180(3, 3);

        visualCube[2 * 12 + 3] = visual[6 * 12 + 5];
        visualCube[2 * 12 + 4] = visual[6 * 12 + 4];
        visualCube[2 * 12 + 5] = visual[6 * 12 + 3];

        visualCube[3 * 12 + 6] = visual[5 * 12 + 2];
        visualCube[4 * 12 + 6] = visual[4 * 12 + 2];
        visualCube[5 * 12 + 6] = visual[3 * 12 + 2];

        visualCube[6 * 12 + 5] = visual[2 * 12 + 3];
        visualCube[6 * 12 + 4] = visual[2 * 12 + 4];
        visualCube[6 * 12 + 3] = visual[2 * 12 + 5];

        visualCube[5 * 12 + 2] = visual[3 * 12 + 6];
        visualCube[4 * 12 + 2] = visual[4 * 12 + 6];
        visualCube[3 * 12 + 2] = visual[5 * 12 + 6];
        return;
    }
    if (mov == "B") {
        rotarCaraHorario(3, 9);

        visualCube[0 * 12 + 3] = visual[3 * 12 + 8];
        visualCube[0 * 12 + 4] = visual[4 * 12 + 8];
        visualCube[0 * 12 + 5] = visual[5 * 12 + 8];

        visualCube[5 * 12 + 0] = visual[0 * 12 + 3];
        visualCube[4 * 12 + 0] = visual[0 * 12 + 4];
        visualCube[3 * 12 + 0] = visual[0 * 12 + 5];

        visualCube[8 * 12 + 3] = visual[3 * 12 + 0];
        visualCube[8 * 12 + 4] = visual[4 * 12 + 0];
        visualCube[8 * 12 + 5] = visual[5 * 12 + 0];

        visualCube[5 * 12 + 8] = visual[8 * 12 + 3];
        visualCube[4 * 12 + 8] = visual[8 * 12 + 4];
        visualCube[3 * 12 + 8] = visual[8 * 12 + 5];
        return;
    }
    if (mov == "B'") {
        rotarCaraAntihorario(3, 9);

        visualCube[0 * 12 + 3] = visual[5 * 12 + 0];
        visualCube[0 * 12 + 4] = visual[4 * 12 + 0];
        visualCube[0 * 12 + 5] = visual[3 * 12 + 0];

        visualCube[3 * 12 + 8] = visual[0 * 12 + 3];
        visualCube[4 * 12 + 8] = visual[0 * 12 + 4];
        visualCube[5 * 12 + 8] = visual[0 * 12 + 5];

        visualCube[8 * 12 + 5] = visual[3 * 12 + 8];
        visualCube[8 * 12 + 4] = visual[4 * 12 + 8];
        visualCube[8 * 12 + 3] = visual[5 * 12 + 8];

        visualCube[3 * 12 + 0] = visual[8 * 12 + 3];
        visualCube[4 * 12 + 0] = visual[8 * 12 + 4];
        visualCube[5 * 12 + 0] = visual[8 * 12 + 5];
        return;
    }
    if (mov == "B2") {
        rotarCara180(3, 9);

        visualCube[0 * 12 + 3] = visual[8 * 12 + 5];
        visualCube[0 * 12 + 4] = visual[8 * 12 + 4];
        visualCube[0 * 12 + 5] = visual[8 * 12 + 3];

        visualCube[3 * 12 + 8] = visual[5 * 12 + 0];
        visualCube[4 * 12 + 8] = visual[4 * 12 + 0];
        visualCube[5 * 12 + 8] = visual[3 * 12 + 0];

        visualCube[8 * 12 + 5] = visual[0 * 12 + 3];
        visualCube[8 * 12 + 4] = visual[0 * 12 + 4];
        visualCube[8 * 12 + 3] = visual[0 * 12 + 5];

        visualCube[5 * 12 + 0] = visual[3 * 12 + 8];
        visualCube[4 * 12 + 0] = visual[4 * 12 + 8];
        visualCube[3 * 12 + 0] = visual[5 * 12 + 8];
        return;
    }
    if (mov == "U") {
        rotarCaraHorario(0, 3);

        visualCube[3 * 12 + 0] = visual[3 * 12 + 3];
        visualCube[3 * 12 + 1] = visual[3 * 12 + 4];
        visualCube[3 * 12 + 2] = visual[3 * 12 + 5];

        visualCube[3 * 12 + 9] = visual[3 * 12 + 0];
        visualCube[3 * 12 + 10] = visual[3 * 12 + 1];
        visualCube[3 * 12 + 11] = visual[3 * 12 + 2];

        visualCube[3 * 12 + 6] = visual[3 * 12 + 9];
        visualCube[3 * 12 + 7] = visual[3 * 12 + 10];
        visualCube[3 * 12 + 8] = visual[3 * 12 + 11];

        visualCube[3 * 12 + 3] = visual[3 * 12 + 6];
        visualCube[3 * 12 + 4] = visual[3 * 12 + 7];
        visualCube[3 * 12 + 5] = visual[3 * 12 + 8];
        return;
    }
    if (mov == "U'") {
        rotarCaraAntihorario(0, 3);

        visualCube[3 * 12 + 0] = visual[3 * 12 + 9];
        visualCube[3 * 12 + 1] = visual[3 * 12 + 10];
        visualCube[3 * 12 + 2] = visual[3 * 12 + 11];

        visualCube[3 * 12 + 3] = visual[3 * 12 + 0];
        visualCube[3 * 12 + 4] = visual[3 * 12 + 1];
        visualCube[3 * 12 + 5] = visual[3 * 12 + 2];

        visualCube[3 * 12 + 6] = visual[3 * 12 + 3];
        visualCube[3 * 12 + 7] = visual[3 * 12 + 4];
        visualCube[3 * 12 + 8] = visual[3 * 12 + 5];

        visualCube[3 * 12 + 9] = visual[3 * 12 + 6];
        visualCube[3 * 12 + 10] = visual[3 * 12 + 7];
        visualCube[3 * 12 + 11] = visual[3 * 12 + 8];
        return;
    }
    if (mov == "U2") {
        rotarCara180(0, 3);

        visualCube[3 * 12 + 0] = visual[3 * 12 + 6];
        visualCube[3 * 12 + 1] = visual[3 * 12 + 7];
        visualCube[3 * 12 + 2] = visual[3 * 12 + 8];

        visualCube[3 * 12 + 3] = visual[3 * 12 + 9];
        visualCube[3 * 12 + 4] = visual[3 * 12 + 10];
        visualCube[3 * 12 + 5] = visual[3 * 12 + 11];

        visualCube[3 * 12 + 6] = visual[3 * 12 + 0];
        visualCube[3 * 12 + 7] = visual[3 * 12 + 1];
        visualCube[3 * 12 + 8] = visual[3 * 12 + 2];

        visualCube[3 * 12 + 9] = visual[3 * 12 + 3];
        visualCube[3 * 12 + 10] = visual[3 * 12 + 4];
        visualCube[3 * 12 + 11] = visual[3 * 12 + 5];
        return;
    }
    if (mov == "D") {
        rotarCaraHorario(6, 3);

        visualCube[5 * 12 + 0] = visual[5 * 12 + 9];
        visualCube[5 * 12 + 1] = visual[5 * 12 + 10];
        visualCube[5 * 12 + 2] = visual[5 * 12 + 11];

        visualCube[5 * 12 + 3] = visual[5 * 12 + 0];
        visualCube[5 * 12 + 4] = visual[5 * 12 + 1];
        visualCube[5 * 12 + 5] = visual[5 * 12 + 2];

        visualCube[5 * 12 + 6] = visual[5 * 12 + 3];
        visualCube[5 * 12 + 7] = visual[5 * 12 + 4];
        visualCube[5 * 12 + 8] = visual[5 * 12 + 5];

        visualCube[5 * 12 + 9] = visual[5 * 12 + 6];
        visualCube[5 * 12 + 10] = visual[5 * 12 + 7];
        visualCube[5 * 12 + 11] = visual[5 * 12 + 8];
        return;
    }
    if (mov == "D'") {
        rotarCaraAntihorario(6, 3);

        visualCube[5 * 12 + 0] = visual[5 * 12 + 3];
        visualCube[5 * 12 + 1] = visual[5 * 12 + 4];
        visualCube[5 * 12 + 2] = visual[5 * 12 + 5];

        visualCube[5 * 12 + 9] = visual[5 * 12 + 0];
        visualCube[5 * 12 + 10] = visual[5 * 12 + 1];
        visualCube[5 * 12 + 11] = visual[5 * 12 + 2];

        visualCube[5 * 12 + 6] = visual[5 * 12 + 9];
        visualCube[5 * 12 + 7] = visual[5 * 12 + 10];
        visualCube[5 * 12 + 8] = visual[5 * 12 + 11];

        visualCube[5 * 12 + 3] = visual[5 * 12 + 6];
        visualCube[5 * 12 + 4] = visual[5 * 12 + 7];
        visualCube[5 * 12 + 5] = visual[5 * 12 + 8];
        return;
    }
    if (mov == "D2") {
        rotarCara180(6, 3);

        visualCube[5 * 12 + 0] = visual[5 * 12 + 6];
        visualCube[5 * 12 + 1] = visual[5 * 12 + 7];
        visualCube[5 * 12 + 2] = visual[5 * 12 + 8];

        visualCube[5 * 12 + 3] = visual[5 * 12 + 9];
        visualCube[5 * 12 + 4] = visual[5 * 12 + 10];
        visualCube[5 * 12 + 5] = visual[5 * 12 + 11];

        visualCube[5 * 12 + 6] = visual[5 * 12 + 0];
        visualCube[5 * 12 + 7] = visual[5 * 12 + 1];
        visualCube[5 * 12 + 8] = visual[5 * 12 + 2];

        visualCube[5 * 12 + 9] = visual[5 * 12 + 3];
        visualCube[5 * 12 + 10] = visual[5 * 12 + 4];
        visualCube[5 * 12 + 11] = visual[5 * 12 + 5];
        return;
    }
}
