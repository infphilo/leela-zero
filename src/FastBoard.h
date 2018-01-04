/*
    This file is part of Leela Zero.
    Copyright (C) 2017 Gian-Carlo Pascutto

    Leela Zero is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    Leela Zero is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Leela Zero.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef FASTBOARD_H_INCLUDED
#define FASTBOARD_H_INCLUDED

#include "config.h"

#include <array>
#include <string>
#include <vector>
#include <queue>
#include <assert.h>

const static int DK_num_stone = 5;

class FastBoard {
    friend class FastState;
public:
    /*
        neighbor counts are up to 4, so 3 bits is ok,
        but a power of 2 makes things a bit faster
    */
    static constexpr int NBR_SHIFT = 4;

    /*
        largest board supported
    */
    static constexpr int MAXBOARDSIZE = 19;

    /*
        highest existing square
    */
    static constexpr int MAXSQ = ((MAXBOARDSIZE + 2) * (MAXBOARDSIZE + 2));

    /*
        infinite score
    */
    static constexpr int BIG = 10000000;

    /*
        vertex of a pass
    */
    static constexpr int PASS   = -1;
    /*
        vertex of a "resign move"
    */
    static constexpr int RESIGN = -2;

    /*
        possible contents of a square
    */
    enum square_t : char {
        BLACK = 0, WHITE = 1, EMPTY = 2, INVAL = 3
    };

    /*
        move generation types
    */
    using movescore_t = std::pair<int, float>;
    using scoredmoves_t = std::vector<movescore_t>;

    int get_boardsize(void) const;
    square_t get_square(int x, int y) const;
    square_t get_square(int vertex) const ;
    int get_vertex(int i, int j) const;
    void set_square(int x, int y, square_t content);
    void set_square(int vertex, square_t content);
    std::pair<int, int> get_xy(int vertex) const;
    
    // DK - five mok win
    bool win(int vertex) const {
        std::pair<int, int> pos = get_xy(vertex);
        square_t color = get_square(pos.first, pos.second);
        assert(color == BLACK || color == WHITE);
        
        int dir[4][2][2] = {
            {{-1,  0}, {1,  0}},
            {{ 0, -1}, {0,  1}},
            {{-1, -1}, {1,  1}},
            {{-1,  1}, {1, -1}}};
        for(int i = 0; i < 4; i++) {
            int count = 1;
            for(int j = 0; j < 2; j++) {
                std::pair<int, int> tpos = pos;
                tpos.first += dir[i][j][0];
                tpos.second += dir[i][j][1];
                while(tpos.first  >= 0 && tpos.first < MAXBOARDSIZE &&
                      tpos.second >= 0 && tpos.second < MAXBOARDSIZE) {
                    square_t tcolor = get_square(tpos.first, tpos.second);
                    if(tcolor != color) break;
                    count += 1;
                    tpos.first += dir[i][j][0];
                    tpos.second += dir[i][j][1];
                }
            }
            if(count >= DK_num_stone)
                return true;
        }

        return false;
    }

    bool is_suicide(int i, int color);
    int count_pliberties(const int i);
    void augment_chain(std::vector<int> & chains, int vertex);
    bool is_eye(const int color, const int vtx);
    int get_dir(int i) const;
    int get_extra_dir(int i) const;

    int estimate_mc_score(float komi);
    float final_mc_score(float komi);
    float area_score(float komi);
    std::vector<bool> calc_reach_color(int col);

    int get_prisoners(int side);
    bool black_to_move();
    int get_to_move();
    void set_to_move(int color);

    std::string move_to_text(int move);
    std::string move_to_text_sgf(int move);
    std::string get_stone_list();
    std::string get_string(int vertex);

    void reset_board(int size);
    void display_board(int lastmove = -1);

    static bool starpoint(int size, int point);
    static bool starpoint(int size, int x, int y);

protected:
    /*
        bit masks to detect eyes on neighbors
    */
    static const std::array<int,      2> s_eyemask;
    static const std::array<square_t, 4> s_cinvert; /* color inversion */

    std::array<square_t, MAXSQ>            m_square;      /* board contents */
    std::array<unsigned short, MAXSQ+1>    m_next;        /* next stone in string */
    std::array<unsigned short, MAXSQ+1>    m_parent;      /* parent node of string */
    std::array<unsigned short, MAXSQ+1>    m_libs;        /* liberties per string parent */
    std::array<unsigned short, MAXSQ+1>    m_stones;      /* stones per string parent */
    std::array<unsigned short, MAXSQ>      m_neighbours;  /* counts of neighboring stones */
    std::array<int, 4>                     m_dirs;        /* movement directions 4 way */
    std::array<int, 8>                     m_extradirs;   /* movement directions 8 way */
    std::array<int, 2>                     m_prisoners;   /* prisoners per color */
    std::array<int, 2>                     m_totalstones; /* stones per color */
    std::vector<int>                       m_critical;    /* queue of critical points */
    std::array<unsigned short, MAXSQ>      m_empty;       /* empty squares */
    std::array<unsigned short, MAXSQ>      m_empty_idx;   /* indexes of square */
    int m_empty_cnt;                                      /* count of empties */

    int m_tomove;
    int m_maxsq;

    int m_boardsize;

    int count_neighbours(const int color, const int i);
    void merge_strings(const int ip, const int aip);
    int remove_string_fast(int i);
    void add_neighbour(const int i, const int color);
    void remove_neighbour(const int i, const int color);
};

#endif
