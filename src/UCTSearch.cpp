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

#include "config.h"

#include <iostream>
#include <iomanip>
#include <assert.h>
#include <limits.h>
#include <cmath>
#include <vector>
#include <utility>
#include <thread>
#include <algorithm>
#include <type_traits>

#include "FastBoard.h"
#include "UCTSearch.h"
#include "Timing.h"
#include "Random.h"
#include "Utils.h"
#include "Network.h"
#include "GTP.h"
#include "TTable.h"
#include "Training.h"
#ifdef USE_OPENCL
#include "OpenCL.h"
#endif

using namespace Utils;

UCTSearch::UCTSearch(GameState & g)
    : m_rootstate(g) {
    set_playout_limit(cfg_max_playouts);
}

SearchResult UCTSearch::play_simulation(GameState & currstate, UCTNode* const node) {
    const auto color = currstate.get_to_move();
    const auto hash = currstate.board.get_hash();
    const auto komi = currstate.get_komi();

    auto result = SearchResult{};

    TTable::get_TT()->sync(hash, komi, node);
    node->virtual_loss();

    if (!node->has_children()) {
        if (currstate.get_passes() >= 2) {
            auto score = currstate.final_score();
            result = SearchResult::from_score(score);
        } else if (m_nodes < MAX_TREE_SIZE) {
            float eval;
            bool noise = false;
            auto success = node->create_children(m_nodes, currstate, eval, noise);
            if (success) {
                result = SearchResult::from_eval(eval);
            }
        } else {
            auto eval = node->eval_state(currstate);
            result = SearchResult::from_eval(eval);
        }
    }

    if (node->has_children() && !result.valid()) {
        auto next = node->uct_select_child(color);
        if (next != nullptr) {
            auto move = next->get_move();
            if (move != FastBoard::PASS) {
                // DK - debugging purposes
                if(currstate.get_movenum() >= 4 && false) {
                    std::pair<int, int> pos = currstate.board.get_xy(move);
                    for(int s = 0; s < currstate.get_movenum(); s++) {
                        std::cerr << " ";
                    }
                    std::cerr << currstate.get_movenum() << ": " << currstate.board.move_to_text(move) << " (" << pos.first << ", " << pos.second << ": " << move << ")" << std::endl;
                    if(currstate.get_movenum() == 2 &&
                       pos.first >= 8 && pos.first <= 10 &&
                       pos.second >= 8 && pos.second <= 10) {
                        int kk = 20;
                        kk += 20;
                    }
                }
                currstate.play_move(move);
                if(!currstate.superko()) {
                    result = play_simulation(currstate, next);
                } else {
                    next->invalidate();
                }
            } else {
                currstate.play_pass();
                result = play_simulation(currstate, next);
            }
        }
    }

    if (result.valid()) {
        node->update(result.eval());
    }
    node->virtual_loss_undo();
    TTable::get_TT()->update(hash, komi, node);

    return result;
}

void UCTSearch::dump_stats(KoState & state, UCTNode & parent) {
    if (cfg_quiet || !parent.has_children() || parent.get_first_child() == nullptr) {
        return;
    }

    const int color = state.get_to_move();

    // sort children, put best move on top
    m_root.sort_root_children(color);

    UCTNode * bestnode = parent.get_first_child();

    if (bestnode->first_visit()) {
        return;
    }

    int movecount = 0;
    UCTNode * node = bestnode;

    while (node != nullptr) {
        if (++movecount > 2 && !node->get_visits()) break;

        std::string tmp = state.move_to_text(node->get_move());
        std::string pvstring(tmp);

        myprintf("%4s -> %7d (V: %5.2f%%) (N: %5.2f%%) PV: ",
            tmp.c_str(),
            node->get_visits(),
            node->get_visits() > 0 ? node->get_eval(color)*100.0f : 0.0f,
            node->get_score() * 100.0f);

        KoState tmpstate = state;

        tmpstate.play_move(node->get_move());
        pvstring += " " + get_pv(tmpstate, *node);

        myprintf("%s\n", pvstring.c_str());

        node = node->get_sibling();
    }
}

int UCTSearch::get_best_move(passflag_t passflag) {
    if(m_root.get_first_child() == nullptr) {
        return FastBoard::PASS;
    }
    int to_move = m_rootstate.board.get_to_move();

    // Make sure best is first
    m_root.sort_root_children(to_move);

    // Check whether to randomize the best move proportional
    // to the playout counts, early game only.
    auto movenum = int(m_rootstate.get_movenum());
    if (movenum < cfg_random_cnt) {
        m_root.randomize_first_proportionally();
    }

    int bestmove = m_root.get_first_child()->get_move();
    
    // DK - better move than bestmove above
#if 1
    float best_mine_winrate = 0.0f, best_enemy_winrate = 0.0f;
    int best_mine_move = -1, best_enemy_move = -1;
    for(int x = 0; x < 19; x++) {
        for(int y = 0; y < 19; y++) {
            int vertex = m_rootstate.board.get_vertex(x, y);
            if(m_rootstate.board.get_square(vertex) != FastBoard::EMPTY)
                continue;
            std::pair<int, int> pos = m_rootstate.board.get_xy(vertex);
            assert(pos.first == x && pos.second == y);
            int dir[4][2] = {{1, 0}, {0, 1}, {1, 1}, {-1,  1}};
            for(int c = 0; c < 2; c++) {
                FastBoard::square_t color = (c == 0 ? FastBoard::BLACK : FastBoard::WHITE);
                int five = 0, four = 0;
                for(int i = 0; i < 4; i++) {
                    int stones[DK_num_stone * 2 - 1] = {0,}; // 0 as empty, 1 as mine, 2 as enemy or wall
                    std::pair<int, int> tmp_pos = pos;
                    tmp_pos.first += (dir[i][0] * -(DK_num_stone - 1));
                    tmp_pos.second += (dir[i][1] * -(DK_num_stone - 1));
                    for(int j = 0; j < DK_num_stone * 2; j++) {
                        if(tmp_pos.first < 0 ||
                           tmp_pos.first >= FastBoard::MAXBOARDSIZE ||
                           tmp_pos.second < 0 ||
                           tmp_pos.second >= FastBoard::MAXBOARDSIZE) {
                            stones[j] = 2;
                        } else {
                            if(tmp_pos == pos) {
                                stones[j] = 1;
                            } else {
                                FastBoard::square_t tcolor = m_rootstate.board.get_square(tmp_pos.first, tmp_pos.second);
                                if(tcolor == color) {
                                    stones[j] = 1;
                                } else if(tcolor == FastBoard::EMPTY) {
                                    stones[j] = 0;
                                } else{
                                    stones[j] = 2;
                                }
                            }
                        }
                        
                        tmp_pos.first += dir[i][0];
                        tmp_pos.second += dir[i][1];
                    }
                    
                    // five
                    for(int j = 0; j < DK_num_stone; j++) {
                        int mine_count = 0, empty_count = 0;
                        for(int k = j; k < j + DK_num_stone; k++) {
                            if(stones[k] == 1) {
                                mine_count++;
                            } else if(stones[k] == 0) {
                                empty_count++;
                            }
                        }
                        if(mine_count == DK_num_stone) {
                            five++;
                        } else if(mine_count == DK_num_stone - 1 && empty_count == 1) {
                            four++;
                        }
                    }
                }
                if(five > 0 || four >= 1) {
                    int rank = 0;
                    UCTNode* temp = m_root.get_first_child();
                    while (temp != NULL) {
                        if(temp->get_move() == vertex) {
                            break;
                        }
                        temp = temp->get_sibling();
                        rank++;
                    }
                    float init_point = (361 - rank) / 1000000.0f;
                    if(color == to_move) {
                        float mine_winrate = init_point;
                        if(five > 0) {
                            mine_winrate += (1.0f + five / 1000.0f);
                        } else if(four > 1) {
                            mine_winrate += (0.99f + four / 1000.0f);
                        } else if(four == 1) {
                            mine_winrate += 0.98f;
                        }
                        if(best_mine_winrate < mine_winrate) {
                            best_mine_move = vertex;
                            best_mine_winrate = mine_winrate;
                        }
                    } else {
                        assert(color != FastBoard::EMPTY);
                        float enemy_winrate = init_point;
                        if(five > 0) {
                            enemy_winrate += (1.0f + five / 1000.0f);
                        } else if(four > 1) {
                            enemy_winrate += (0.99f + four / 1000.0f);
                        } else if(four == 1) {
                            enemy_winrate += 0.98f;
                        }
                        if(best_enemy_winrate < enemy_winrate) {
                            best_enemy_move = vertex;
                            best_enemy_winrate = enemy_winrate;
                        }
                    }
                }
            }
        }
    }
    if(best_mine_winrate >= 0.99f || best_enemy_winrate >= 0.99f) {
        bool must_play = true;
        if(best_mine_winrate >= 1.0f) {
            bestmove = best_mine_move;
        } else if(best_enemy_winrate >= 1.0f) {
            bestmove = best_enemy_move;
        } else if(best_mine_winrate >= 0.99f) {
            bestmove = best_mine_move;
        } else if(best_enemy_winrate >= 0.99f && best_mine_winrate < 0.98f) {
            bestmove = best_enemy_move;
        } else {
            must_play = false;
        }
        if(must_play) {
            return bestmove;
        }
    }
#endif

    // do we have statistics on the moves?
    if (m_root.get_first_child() != nullptr) {
        if (m_root.get_first_child()->first_visit()) {
            return bestmove;
        }
    }

    float bestscore = m_root.get_first_child()->get_eval(to_move);

    // do we want to fiddle with the best move because of the rule set?
    if (passflag & UCTSearch::NOPASS) {
        // were we going to pass?
        if (bestmove == FastBoard::PASS) {
            UCTNode * nopass = m_root.get_nopass_child(m_rootstate);

            if (nopass != nullptr) {
                myprintf("Preferring not to pass.\n");
                bestmove = nopass->get_move();
                if (nopass->first_visit()) {
                    bestscore = 1.0f;
                } else {
                    bestscore = nopass->get_eval(to_move);
                }
            } else {
                myprintf("Pass is the only acceptable move.\n");
            }
        }
    } else {
        if (!cfg_dumbpass && bestmove == FastBoard::PASS) {
            // Either by forcing or coincidence passing is
            // on top...check whether passing loses instantly
            // do full count including dead stones.
            // In a reinforcement learning setup, it is possible for the
            // network to learn that, after passing in the tree, the two last
            // positions are identical, and this means the position is only won
            // if there are no dead stones in our own territory (because we use
            // Trump-Taylor scoring there). So strictly speaking, the next
            // heuristic isn't required for a pure RL network, and we have
            // a commandline option to disable the behavior during learning.
            // On the other hand, with a supervised learning setup, we fully
            // expect that the engine will pass out anything that looks like
            // a finished game even with dead stones on the board (because the
            // training games were using scoring with dead stone removal).
            // So in order to play games with a SL network, we need this
            // heuristic so the engine can "clean up" the board. It will still
            // only clean up the bare necessity to win. For full dead stone
            // removal, kgs-genmove_cleanup and the NOPASS mode must be used.
            float score = m_rootstate.final_score();
            // Do we lose by passing?
            if ((score > 0.0f && to_move == FastBoard::WHITE)
                ||
                (score < 0.0f && to_move == FastBoard::BLACK)) {
                myprintf("Passing loses :-(\n");
                // Find a valid non-pass move.
                UCTNode * nopass = m_root.get_nopass_child(m_rootstate);
                if (nopass != nullptr) {
                    myprintf("Avoiding pass because it loses.\n");
                    bestmove = nopass->get_move();
                    if (nopass->first_visit()) {
                        bestscore = 1.0f;
                    } else {
                        bestscore = nopass->get_eval(to_move);
                    }
                } else {
                    myprintf("No alternative to passing.\n");
                }
            } else {
                myprintf("Passing wins :-)\n");
            }
        } else if (!cfg_dumbpass
                   && m_rootstate.get_last_move() == FastBoard::PASS) {
            // Opponents last move was passing.
            // We didn't consider passing. Should we have and
            // end the game immediately?
            float score = m_rootstate.final_score();
            // do we lose by passing?
            if ((score > 0.0f && to_move == FastBoard::WHITE)
                ||
                (score < 0.0f && to_move == FastBoard::BLACK)) {
                myprintf("Passing loses, I'll play on.\n");
            } else {
                myprintf("Passing wins, I'll pass out.\n");
                bestmove = FastBoard::PASS;
            }
        }
    }

    int visits = m_root.get_visits();

    // if we aren't passing, should we consider resigning?
    if (bestmove != FastBoard::PASS) {
        // resigning allowed
        if ((passflag & UCTSearch::NORESIGN) == 0) {
            size_t movetresh = (m_rootstate.board.get_boardsize()
                                * m_rootstate.board.get_boardsize()) / 4;
            // bad score and visited enough
            if (bestscore < ((float)cfg_resignpct / 100.0f)
                && visits > 500
                && m_rootstate.m_movenum > movetresh) {
                myprintf("Score looks bad. Resigning.\n");
                bestmove = FastBoard::RESIGN;
            }
        }
    }

    return bestmove;
}

std::string UCTSearch::get_pv(KoState & state, UCTNode & parent) {
    if (!parent.has_children() || parent.get_first_child() == nullptr) {
        return std::string();
    }

    auto best_child = parent.get_best_root_child(state.get_to_move());
    auto best_move = best_child->get_move();
    auto res = state.move_to_text(best_move);

    state.play_move(best_move);

    auto next = get_pv(state, *best_child);
    if (!next.empty()) {
        res.append(" ").append(next);
    }
    return res;
}

void UCTSearch::dump_analysis(int playouts) {
    if (cfg_quiet) {
        return;
    }

    GameState tempstate = m_rootstate;
    int color = tempstate.board.get_to_move();

    std::string pvstring = get_pv(tempstate, m_root);
    float winrate = 100.0f * m_root.get_eval(color);
    myprintf("Playouts: %d, Win: %5.2f%%, PV: %s\n",
             playouts, winrate, pvstring.c_str());
}

bool UCTSearch::is_running() const {
    return m_run;
}

bool UCTSearch::playout_limit_reached() const {
    return m_playouts >= m_maxplayouts;
}

void UCTWorker::operator()() {
    do {
        auto currstate = std::make_unique<GameState>(m_rootstate);
        auto result = m_search->play_simulation(*currstate, m_root);
        if (result.valid()) {
            m_search->increment_playouts();
        }
    } while(m_search->is_running() && !m_search->playout_limit_reached());
}

void UCTSearch::increment_playouts() {
    m_playouts++;
}

int UCTSearch::think(int color, passflag_t passflag) {
    assert(m_playouts == 0);
    assert(m_nodes == 0);

    // Start counting time for us
    m_rootstate.start_clock(color);

    // set side to move
    m_rootstate.board.set_to_move(color);

    // set up timing info
    Time start;

    m_rootstate.get_timecontrol().set_boardsize(m_rootstate.board.get_boardsize());
    auto time_for_move = m_rootstate.get_timecontrol().max_time_for_move(color);

    myprintf("Thinking at most %.1f seconds...\n", time_for_move/100.0f);

    // create a sorted list off legal moves (make sure we
    // play something legal and decent even in time trouble)
    float root_eval;
    bool noise = cfg_noise;
    m_root.create_children(m_nodes, m_rootstate, root_eval, noise);
    // m_root.kill_superkos(m_rootstate);
    if (cfg_noise && noise) {
        m_root.dirichlet_noise(0.25f, 0.03f);
    }
    
    // DK - debugging purposes
#if 0
    UCTNode* temp = m_root.get_first_child();
    std::pair<int, int> ranks[19][19];
    for(int y = 0; y < 19; y++) {
        for(int x = 0; x < 19; x++) {
            ranks[y][x] = std::make_pair<int, int>(0, 0);
        }
    }
    int rank = 1;
    while (temp != NULL) {
        std::pair<int, int> pos = m_rootstate.board.get_xy(temp->get_move());
        int x = pos.first, y = pos.second;
        ranks[y][x] = std::make_pair<int, int>(rank++, (int)(temp->get_score() * 10000));
        temp = temp->get_sibling();
    }
    
    std::cerr << "   ";
    for(int x = 0; x < 19; x++) {
        std::cerr << std::setw(4) << "abcdefghjklmnopqrst"[x] << " ";
    }
    std::cerr << std::endl;
    for(int y = 0; y < 19; y++) {
        std::cerr << std::setw(2) << 19 - y << " ";
        for(int x = 0; x < 19; x++) {
            std::cerr << std::setw(4) << ranks[19 - y - 1][x].first << " ";
        }
        std::cerr << "\n";
    }
    std::cerr << "\n\n";
    std::cerr << "   ";
    for(int x = 0; x < 19; x++) {
        std::cerr << std::setw(4) << "abcdefghjklmnopqrst"[x] << " ";
    }
    std::cerr << std::endl;
    for(int y = 0; y < 19; y++) {
        std::cerr << std::setw(2) << 19 - y << " ";
        for(int x = 0; x < 19; x++) {
            std::cerr << std::setw(4) << ranks[19 - y - 1][x].second << " ";
        }
        std::cerr << "\n";
    }
#endif

    myprintf("NN eval=%f\n",
             (color == FastBoard::BLACK ? root_eval : 1.0f - root_eval));

    m_run = true;
    int cpus = cfg_num_threads;
    ThreadGroup tg(thread_pool);
    for (int i = 1; i < cpus; i++) {
        tg.add_task(UCTWorker(m_rootstate, this, &m_root));
    }

    bool keeprunning = true;
    int last_update = 0;
    do {
        auto currstate = std::make_unique<GameState>(m_rootstate);

        auto result = play_simulation(*currstate, &m_root);
        if (result.valid()) {
            increment_playouts();
        }

        Time elapsed;
        int centiseconds_elapsed = Time::timediff(start, elapsed);

        // output some stats every few seconds
        // check if we should still search
        if (centiseconds_elapsed - last_update > 250) {
            last_update = centiseconds_elapsed;
            dump_analysis(static_cast<int>(m_playouts));
            break;
        }
        keeprunning  = is_running();
        keeprunning &= (centiseconds_elapsed < time_for_move);
        keeprunning &= !playout_limit_reached();
    } while(keeprunning);

    // stop the search
    m_run = false;
    tg.wait_all();
    m_rootstate.stop_clock(color);
    if (!m_root.has_children()) {
        return FastBoard::PASS;
    }

    // display search info
    myprintf("\n");

    dump_stats(m_rootstate, m_root);
    Training::record(m_rootstate, m_root);

    Time elapsed;
    int centiseconds_elapsed = Time::timediff(start, elapsed);
    if (centiseconds_elapsed > 0) {
        myprintf("%d visits, %d nodes, %d playouts, %d n/s\n\n",
                 m_root.get_visits(),
                 static_cast<int>(m_nodes),
                 static_cast<int>(m_playouts),
                 (m_playouts * 100) / (centiseconds_elapsed+1));
    }
    int bestmove = get_best_move(passflag);
    return bestmove;
}

void UCTSearch::ponder() {
    assert(m_playouts == 0);
    assert(m_nodes == 0);

    m_run = true;
    int cpus = cfg_num_threads;
    ThreadGroup tg(thread_pool);
    for (int i = 1; i < cpus; i++) {
        tg.add_task(UCTWorker(m_rootstate, this, &m_root));
    }
    do {
        auto currstate = std::make_unique<GameState>(m_rootstate);
        auto result = play_simulation(*currstate, &m_root);
        if (result.valid()) {
            increment_playouts();
        }
    } while(!Utils::input_pending() && is_running());

    // stop the search
    m_run = false;
    tg.wait_all();
    // display search info
    myprintf("\n");
    dump_stats(m_rootstate, m_root);

    myprintf("\n%d visits, %d nodes\n\n", m_root.get_visits(), (int)m_nodes);
}

void UCTSearch::set_playout_limit(int playouts) {
    static_assert(std::is_convertible<decltype(playouts),
                                      decltype(m_maxplayouts)>::value,
                  "Inconsistent types for playout amount.");
    if (playouts == 0) {
        m_maxplayouts = std::numeric_limits<decltype(m_maxplayouts)>::max();
    } else {
        m_maxplayouts = playouts;
    }
}
