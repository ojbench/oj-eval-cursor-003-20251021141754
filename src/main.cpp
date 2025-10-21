#include <bits/stdc++.h>
using namespace std;

struct ProblemState {
    // before freeze tracking
    int wrong_before = 0;           // wrong attempts before first AC or before freeze
    bool solved = false;            // solved before freeze?
    int first_ac_time = 0;          // time of first AC (if solved before freeze)

    // freeze tracking
    bool frozen = false;            // is this problem currently frozen for the team
    int submissions_after_freeze = 0; // number of submissions after freeze

    // scrolling resolution (when frozen)
    int wrong_in_freeze = 0;        // wrong submissions after freeze before first AC during freeze
    bool ac_in_freeze = false;      // did AC occur during freeze period
    int ac_time_in_freeze = 0;      // time of AC inside freeze
};

struct TeamState {
    string name;

    // live scoreboard (visible, only from non-frozen, i.e., pre-freeze events that were flushed)
    int solved_visible = 0;         // visible solved count
    long long penalty_visible = 0;  // visible total penalty
    vector<int> solve_times_visible; // multiset as vector for tie-breaker (descending comparison by max then ...)

    // per-problem state
    vector<ProblemState> problems;  // indexed 0..M-1

    // helper computed each flush: ranking index after last FLUSH
    int last_ranking = 0;
};

struct Submission {
    int team_id;
    int prob_idx; // 0..M-1
    string status; // Accepted, Wrong_Answer, Runtime_Error, Time_Limit_Exceed
    int time;
};

static inline int probIndexFromChar(char c){ return c - 'A'; }

static bool teamRankLess(const TeamState &ta, const TeamState &tb){
    if (ta.solved_visible != tb.solved_visible) return ta.solved_visible > tb.solved_visible;
    if (ta.penalty_visible != tb.penalty_visible) return ta.penalty_visible < tb.penalty_visible;
    const auto &A = ta.solve_times_visible;
    const auto &B = tb.solve_times_visible;
    size_t n = max(A.size(), B.size());
    for (size_t i = 0; i < n; ++i){
        int va = (i < A.size() ? A[i] : 0);
        int vb = (i < B.size() ? B[i] : 0);
        if (va != vb) return va < vb;
    }
    return ta.name < tb.name;
}

struct SystemState {
    bool started = false;
    bool frozen = false; // whether scoreboard is currently frozen
    int duration = 0;
    int problem_count = 0;

    vector<TeamState> teams;                // all teams
    unordered_map<string,int> name_to_id;   // map name -> team index

    vector<Submission> submissions;         // all submissions in chronological order

    // "live" scoreboard snapshot inputs come from teams[*].{solved_visible, penalty_visible, solve_times_visible}
};

// no materialized key needed; compare teams directly

static void computeOrder(const SystemState &S, vector<int> &order){
    int n = (int)S.teams.size();
    order.resize(n);
    iota(order.begin(), order.end(), 0);
    sort(order.begin(), order.end(), [&](int a, int b){
        return teamRankLess(S.teams[a], S.teams[b]);
    });
}

static void setLastRankingFromOrder(SystemState &S, const vector<int> &order){
    for (int i = 0; i < (int)order.size(); ++i){
        S.teams[order[i]].last_ranking = i + 1;
    }
}

static void flushScoreboard(SystemState &S){
    // just output the info line; ranking is maintained via recompute when needed
    cout << "[Info]Flush scoreboard.\n";
}

static void printScoreboard(const SystemState &S, const vector<int> &order){
    for (int idx : order){
        const TeamState &t = S.teams[idx];
        cout << t.name << ' ' << t.last_ranking << ' ' << t.solved_visible << ' ' << t.penalty_visible;
        // per problem status
        for (int p = 0; p < S.problem_count; ++p){
            const ProblemState &ps = t.problems[p];
            cout << ' ';
            if (ps.frozen){
                int x = ps.wrong_before;
                int y = ps.submissions_after_freeze;
                if (x == 0) cout << "0/" << y;
                else cout << '-' << x << '/' << y;
            } else if (ps.solved){
                int x = ps.wrong_before;
                if (x == 0) cout << '+'; else cout << '+' << x;
            } else {
                int x = ps.wrong_before;
                if (x == 0) cout << '.'; else cout << '-' << x;
            }
        }
        cout << '\n';
    }
}

int main(){
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    SystemState S;

    string cmd;
    while (cin >> cmd){
        if (cmd == "ADDTEAM"){
            string team;
            cin >> team;
            if (S.started){
                cout << "[Error]Add failed: competition has started.\n";
                continue;
            }
            if (S.name_to_id.count(team)){
                cout << "[Error]Add failed: duplicated team name.\n";
                continue;
            }
            TeamState t;
            t.name = team;
            t.solved_visible = 0;
            t.penalty_visible = 0;
            t.solve_times_visible.clear();
            S.name_to_id[team] = (int)S.teams.size();
            S.teams.push_back(move(t));
            cout << "[Info]Add successfully.\n";
        } else if (cmd == "START"){
            string kw1, kw2; // DURATION, PROBLEM
            int duration_tmp = 0, problem_cnt_tmp = 0;
            cin >> kw1 >> duration_tmp >> kw2 >> problem_cnt_tmp;
            if (S.started){
                cout << "[Error]Start failed: competition has started.\n";
                continue;
            }
            S.started = true;
            S.duration = duration_tmp;
            S.problem_count = problem_cnt_tmp;
            // initialize problems per team
            for (auto &t : S.teams){
                t.problems.assign(S.problem_count, ProblemState());
                t.solve_times_visible.clear();
            }
            // Before first FLUSH, rankings are lexicographic by team name
            vector<int> lex_order((int)S.teams.size());
            iota(lex_order.begin(), lex_order.end(), 0);
            sort(lex_order.begin(), lex_order.end(), [&](int a, int b){ return S.teams[a].name < S.teams[b].name; });
            setLastRankingFromOrder(S, lex_order);
            cout << "[Info]Competition starts.\n";
        } else if (cmd == "SUBMIT"){
            string prob_name, by_kw, team_name, with_kw, status, at_kw; int time;
            cin >> prob_name >> by_kw >> team_name >> with_kw >> status >> at_kw >> time;
            int team_id = S.name_to_id[team_name];
            int pidx = probIndexFromChar(prob_name[0]);
            Submission sub{team_id, pidx, status, time};
            S.submissions.push_back(sub);

            TeamState &t = S.teams[team_id];
            ProblemState &ps = t.problems[pidx];

            if (!S.frozen){
                // live update to visible if not frozen state
                if (!ps.solved){
                    if (status == "Accepted"){
                        ps.solved = true;
                        ps.first_ac_time = time;
                        // penalty: 20 * wrong_before + first_ac_time
                        t.solved_visible += 1;
                        t.penalty_visible += 20LL * ps.wrong_before + ps.first_ac_time;
                        t.solve_times_visible.push_back(ps.first_ac_time);
                        sort(t.solve_times_visible.begin(), t.solve_times_visible.end(), greater<int>());
                    } else {
                        // any non-AC counts as wrong attempt
                        ps.wrong_before += 1;
                    }
                }
            } else {
                // frozen period: update per-problem frozen fields
                if (!ps.solved){
                    // if not solved before freeze, this problem becomes or stays frozen
                    if (!ps.frozen){
                        ps.frozen = true;
                    }
                    ps.submissions_after_freeze += 1;
                    if (status == "Accepted"){
                        if (!ps.ac_in_freeze){
                            ps.ac_in_freeze = true;
                            ps.ac_time_in_freeze = time;
                        }
                    } else {
                        if (!ps.ac_in_freeze){
                            ps.wrong_in_freeze += 1;
                        }
                    }
                }
            }
        } else if (cmd == "FLUSH"){
            // Recompute ranking and update last_ranking, then print info
            vector<int> order;
            computeOrder(S, order);
            setLastRankingFromOrder(S, order);
            flushScoreboard(S);
        } else if (cmd == "FREEZE"){
            if (S.frozen){
                cout << "[Error]Freeze failed: scoreboard has been frozen.\n";
            } else {
                S.frozen = true;
                cout << "[Info]Freeze scoreboard.\n";
            }
        } else if (cmd == "SCROLL"){
            if (!S.frozen){
                cout << "[Error]Scroll failed: scoreboard has not been frozen.\n";
                continue;
            }
            cout << "[Info]Scroll scoreboard.\n";
            // First, compute initial order and print scoreboard before scrolling
            vector<int> order;
            computeOrder(S, order);
            setLastRankingFromOrder(S, order);
            printScoreboard(S, order);

            int n = (int)S.teams.size();
            // Build frozen list per team (sorted)
            vector<vector<int>> frozen_list(n);
            for (int i = 0; i < n; ++i){
                for (int p = 0; p < S.problem_count; ++p){
                    if (S.teams[i].problems[p].frozen) frozen_list[i].push_back(p);
                }
                if (!frozen_list[i].empty()) sort(frozen_list[i].begin(), frozen_list[i].end());
            }

            // Position map for current order
            vector<int> pos(n, 0);
            for (int i = 0; i < n; ++i) pos[order[i]] = i;

            using PII = pair<int,int>; // (pos, team)
            priority_queue<PII> pq;
            for (int i = 0; i < n; ++i) if (!frozen_list[i].empty()) pq.emplace(pos[i], i);

            auto apply_unfreeze = [&](int team_id, int pidx){
                TeamState &t = S.teams[team_id];
                ProblemState &ps = t.problems[pidx];
                if (!ps.solved){
                    ps.wrong_before += ps.wrong_in_freeze;
                    if (ps.ac_in_freeze){
                        ps.solved = true;
                        ps.first_ac_time = ps.ac_time_in_freeze;
                        t.solved_visible += 1;
                        t.penalty_visible += 20LL * ps.wrong_before + ps.first_ac_time;
                        t.solve_times_visible.push_back(ps.first_ac_time);
                        sort(t.solve_times_visible.begin(), t.solve_times_visible.end(), greater<int>());
                    }
                }
                ps.frozen = false;
                ps.submissions_after_freeze = 0;
                ps.wrong_in_freeze = 0;
                ps.ac_in_freeze = false;
                ps.ac_time_in_freeze = 0;
            };

            while (!pq.empty()){
                auto [pcur, team] = pq.top(); pq.pop();
                if (frozen_list[team].empty()) continue; // stale
                if (pcur != pos[team]) { pq.emplace(pos[team], team); continue; }

                int old_pos = pos[team];
                int pidx = frozen_list[team].front();
                frozen_list[team].erase(frozen_list[team].begin());

                // Apply result
                apply_unfreeze(team, pidx);

                // Binary search new position in [0..old_pos]
                int lo = 0, hi = old_pos, ins = old_pos;
                while (lo <= hi){
                    int mid = (lo + hi) >> 1;
                    int other = order[mid];
                    if (teamRankLess(S.teams[team], S.teams[other])) { ins = mid; hi = mid - 1; }
                    else lo = mid + 1;
                }

                if (ins < old_pos){
                    int replaced_id = order[ins];
                    cout << S.teams[team].name << ' ' << S.teams[replaced_id].name << ' ' << S.teams[team].solved_visible << ' ' << S.teams[team].penalty_visible << "\n";
                    // shift to move team upward
                    for (int i = old_pos; i > ins; --i){
                        order[i] = order[i-1];
                        pos[order[i]] = i;
                    }
                    order[ins] = team;
                    pos[team] = ins;
                }

                if (!frozen_list[team].empty()) pq.emplace(pos[team], team);
            }

            // Final scoreboard
            setLastRankingFromOrder(S, order);
            printScoreboard(S, order);
            S.frozen = false;
        } else if (cmd == "QUERY_RANKING"){
            string team; cin >> team;
            if (!S.name_to_id.count(team)){
                cout << "[Error]Query ranking failed: cannot find the team.\n";
            } else {
                cout << "[Info]Complete query ranking.\n";
                if (S.frozen) cout << "[Warning]Scoreboard is frozen. The ranking may be inaccurate until it were scrolled.\n";
                int id = S.name_to_id[team];
                cout << S.teams[id].name << " NOW AT RANKING " << S.teams[id].last_ranking << "\n";
            }
        } else if (cmd == "QUERY_SUBMISSION"){
            string team, where_kw;
            cin >> team >> where_kw;
            if (!S.name_to_id.count(team)){
                cout << "[Error]Query submission failed: cannot find the team.\n";
            } else {
                cout << "[Info]Complete query submission.\n";
                int tid = S.name_to_id[team];
                // Expect: PROBLEM=... AND STATUS=...
                string prob_token, and_kw, status_token;
                cin >> prob_token >> and_kw >> status_token;
                // Parse tokens like PROBLEM=ALL and STATUS=Accepted
                auto parse_kv = [](const string &tok, const string &prefix)->string{
                    if (tok.size() >= prefix.size()+1 && tok.rfind(prefix, 0) == 0) {
                        return tok.substr(prefix.size());
                    }
                    return string();
                };
                string prob_val = parse_kv(prob_token, "PROBLEM=");
                string status_val = parse_kv(status_token, "STATUS=");
                int target_prob = -1;
                if (!prob_val.empty() && prob_val != "ALL") target_prob = probIndexFromChar(prob_val[0]);
                string target_status = status_val.empty() ? string("ALL") : status_val;
                // Find last submission satisfying conditions (including after freeze)
                int found_idx = -1;
                for (int i = (int)S.submissions.size()-1; i >= 0; --i){
                    const auto &sub = S.submissions[i];
                    if (sub.team_id != tid) continue;
                    if (target_prob != -1 && sub.prob_idx != target_prob) continue;
                    if (target_status != "ALL" && sub.status != target_status) continue;
                    found_idx = i; break;
                }
                if (found_idx == -1){
                    cout << "Cannot find any submission.\n";
                } else {
                    const auto &sub = S.submissions[found_idx];
                    cout << S.teams[tid].name << ' ' << char('A'+sub.prob_idx) << ' ' << sub.status << ' ' << sub.time << "\n";
                }
            }
        } else if (cmd == "END"){
            cout << "[Info]Competition ends.\n";
            break;
        } else {
            // skip unknown (input guaranteed valid format though)
            string line; getline(cin, line);
        }
    }
    return 0;
}
