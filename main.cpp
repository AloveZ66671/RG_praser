#include <algorithm>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <queue>
#include <array>
#include <string>
#include <utility>
using namespace std;

static const char EPS = '\0'; ///< 表示ε空转换的特殊字符

//-------------------- Regex Parser --------------------

/**
 * @brief 正则表达式语法树节点
 *
 * 表示正则表达式的抽象语法树节点类型，每个节点可表示字符、连接、并集和 Kleene星运算。
 */
struct RegexNode {
    /**
     * @brief 正则节点类型枚举
     */
    enum Type { CHAR, CONCAT, UNION, STAR } type; ///< 节点类型

    char ch; ///< 对于CHAR类型的节点所表示的字符
    RegexNode *left, *right; ///< 左右子节点，用于表示CONCAT、UNION等结构

    /**
     * @brief 构造函数
     *
     * @param t 节点类型
     * @param c 对于CHAR类型的字符
     * @param l 左子节点
     * @param r 右子节点
     */
    RegexNode(Type t, char c = 0, RegexNode *l = nullptr, RegexNode *r = nullptr)
        : type(t), ch(c), left(l), right(r) {
    }
};

/**
 * @brief 正则表达式解析器
 *
 * 使用递归下降解析将输入的正则表达式字符串解析为语法树。
 */
class RegexParser {
public:
    /**
     * @brief 构造函数
     *
     * @param s 输入的正则表达式字符串
     */
    explicit RegexParser(string s) : str(std::move(s)), pos(0) {
    }

    /**
     * @brief 解析整个正则表达式
     *
     * @return RegexNode* 解析得到的语法树根节点
     */
    RegexNode *parse() {
        return parseUnion();
    }

private:
    string str; ///< 输入的正则表达式字符串
    int pos;    ///< 当前解析位置

    /**
     * @brief 解析包含 '+' 的并运算
     *
     * @return RegexNode* 对应的语法树节点
     */
    RegexNode *parseUnion() {
        RegexNode *node = parseConcat();
        while (pos < (int) str.size() && str[pos] == '+') {
            pos++;
            RegexNode *right = parseConcat();
            node = new RegexNode(RegexNode::UNION, 0, node, right);
        }
        return node;
    }

    /**
     * @brief 解析连接运算
     *
     * @return RegexNode* 对应的语法树节点
     */
    RegexNode *parseConcat() {
        RegexNode *node = parseStar();
        while (pos < (int) str.size() && (str[pos] != ')' && str[pos] != '+')) {
            RegexNode *right = parseStar();
            node = new RegexNode(RegexNode::CONCAT, 0, node, right);
        }
        return node;
    }

    /**
     * @brief 解析 Kleene 星运算（*）
     *
     * @return RegexNode* 对应的语法树节点
     */
    RegexNode *parseStar() {
        RegexNode *node = parseBase();
        while (pos < (int) str.size() && str[pos] == '*') {
            pos++;
            node = new RegexNode(RegexNode::STAR, 0, node, nullptr);
        }
        return node;
    }

    /**
     * @brief 解析基本单元（字符或括号表达式）
     *
     * @return RegexNode* 对应的语法树节点
     */
    RegexNode *parseBase() {
        if (pos >= (int) str.size()) return nullptr;
        if (str[pos] == '(') {
            pos++;
            RegexNode *node = parseUnion();
            if (pos < (int) str.size() && str[pos] == ')') {
                pos++;
            }
            return node;
        } else if (str[pos] == '0' || str[pos] == '1') {
            RegexNode *node = new RegexNode(RegexNode::CHAR, str[pos]);
            pos++;
            return node;
        }
        return nullptr;
    }
};

//-------------------- NFA定义 --------------------

/**
 * @brief 非确定性有限自动机(NFA)定义结构
 */
struct NFA {
    /**
     * @brief NFA状态定义
     */
    struct State {
        int id; ///< 状态编号
        bool accept = false; ///< 是否为接受态
        map<char, vector<int> > trans; ///< 转移函数：字符到下一状态集合的映射
    };

    vector<State> states; ///< 状态集合
    int start; ///< 起始状态ID

    /**
     * @brief 创建新状态
     *
     * @param accept 是否为接受态
     * @return int 新状态的ID
     */
    int new_state(bool accept = false) {
        State s;
        s.id = (int) states.size();
        s.accept = accept;
        states.push_back(s);
        return s.id;
    }
};

/**
 * @brief NFA片段结构，用于Thompson构造法中间表示
 */
struct NFAFragment {
    int start;  ///< 片段起始状态
    int accept; ///< 片段接受状态
};

/**
 * @brief 使用Thompson构造法将正则语法树转换为ε-NFA
 */
class Thompson {
public:
    /**
     * @brief 构造函数
     *
     * @param root 正则表达式语法树根节点
     */
    Thompson(RegexNode *root) : r(root) {
    }

    /**
     * @brief 构建ε-NFA
     *
     * @return NFA 构建完成的NFA
     */
    NFA build() {
        nfa = NFA();
        NFAFragment frag = buildFragment(r);
        nfa.states[frag.accept].accept = true;
        nfa.start = frag.start;
        return nfa;
    }

private:
    RegexNode *r; ///< 正则表达式语法树根节点
    NFA nfa;       ///< 构建中的NFA

    /**
     * @brief 添加状态转移
     *
     * @param from 源状态
     * @param to 目标状态
     * @param c 转移字符
     */
    void add_transition(int from, int to, char c) {
        nfa.states[from].trans[c].push_back(to);
    }

    /**
     * @brief 针对给定的正则节点构建NFA片段
     *
     * @param node 正则节点
     * @return NFAFragment 对应的NFA片段
     */
    NFAFragment buildFragment(RegexNode *node) {
        switch (node->type) {
            case RegexNode::CHAR: {
                int s = nfa.new_state();
                int a = nfa.new_state();
                add_transition(s, a, node->ch);
                return {s, a};
            }
            case RegexNode::CONCAT: {
                NFAFragment f1 = buildFragment(node->left);
                NFAFragment f2 = buildFragment(node->right);
                add_transition(f1.accept, f2.start, EPS);
                return {f1.start, f2.accept};
            }
            case RegexNode::UNION: {
                int s = nfa.new_state();
                int a = nfa.new_state();
                NFAFragment f1 = buildFragment(node->left);
                NFAFragment f2 = buildFragment(node->right);
                add_transition(s, f1.start, EPS);
                add_transition(s, f2.start, EPS);
                add_transition(f1.accept, a, EPS);
                add_transition(f2.accept, a, EPS);
                return {s, a};
            }
            case RegexNode::STAR: {
                int s = nfa.new_state();
                int a = nfa.new_state();
                NFAFragment f = buildFragment(node->left);
                add_transition(s, f.start, EPS);
                add_transition(f.accept, a, EPS);
                add_transition(s, a, EPS);
                add_transition(f.accept, f.start, EPS);
                return {s, a};
            }
        }
        return {-1, -1};
    }
};

//-------------------- ε-NFA -> NFA --------------------

/**
 * @brief ε-NFA到NFA的转换类
 *
 * 消除ε转移，构建无ε的NFA。
 */
class EpsilonRemover {
public:
    /**
     * @brief 构造函数
     *
     * @param n 输入的ε-NFA
     */
    EpsilonRemover(const NFA &n) : infa(n) {
    }

    /**
     * @brief 消除ε转移
     *
     * @return NFA 无ε转移的NFA
     */
    NFA remove() {
        eps_closures.resize(infa.states.size());
        for (int i = 0; i < (int) infa.states.size(); i++) {
            compute_epsilon_closure(i);
        }

        NFA onfa;
        onfa.states.resize(infa.states.size());
        for (int i = 0; i < (int) infa.states.size(); i++) {
            onfa.states[i].id = i;
        }
        onfa.start = infa.start;

        for (int i = 0; i < (int) infa.states.size(); i++) {
            set<int> closure = eps_closures[i];
            bool is_accept = false;
            map<char, set<int> > combined;
            for (int cst: closure) {
                if (infa.states[cst].accept) is_accept = true;
                for (auto &kv: infa.states[cst].trans) {
                    char c = kv.first;
                    if (c == EPS) continue;
                    for (int nxt: kv.second) {
                        for (int ec: eps_closures[nxt]) {
                            combined[c].insert(ec);
                        }
                    }
                }
            }
            onfa.states[i].accept = is_accept;
            for (auto &kv: combined) {
                onfa.states[i].trans[kv.first] = vector<int>(kv.second.begin(), kv.second.end());
            }
        }

        return onfa;
    }

private:
    const NFA &infa; ///< 输入的ε-NFA
    vector<set<int> > eps_closures; ///< 每个状态的ε闭包

    /**
     * @brief 计算状态s的ε闭包
     *
     * @param s 状态ID
     */
    void compute_epsilon_closure(int s) {
        if (!eps_closures[s].empty()) return;
        stack<int> st;
        st.push(s);
        eps_closures[s].insert(s);
        while (!st.empty()) {
            int u = st.top();
            st.pop();
            auto it = infa.states[u].trans.find(EPS);
            if (it != infa.states[u].trans.end()) {
                for (int nxt: it->second) {
                    if (eps_closures[s].find(nxt) == eps_closures[s].end()) {
                        eps_closures[s].insert(nxt);
                        st.push(nxt);
                    }
                }
            }
        }
    }
};

//-------------------- NFA -> DFA (子集构造) --------------------

/**
 * @brief DFA定义结构
 */
struct DFA {
    /**
     * @brief DFA状态定义
     */
    struct State {
        int id;       ///< 状态编号
        bool accept;  ///< 是否为接受态
        int t0, t1;   ///< 输入字符0或1时的转移状态ID
    };

    vector<State> states; ///< 状态集合
    int start; ///< 起始状态
    int trap;  ///< 陷阱态

    DFA() : start(-1), trap(-1) {
    }
};

/**
 * @brief 子集构造法将NFA转换为DFA的类
 */
class SubsetConstruction {
public:
    /**
     * @brief 构造函数
     *
     * @param n 输入的无ε NFA
     */
    SubsetConstruction(const NFA &n) : infa(n) {
    }

    /**
     * @brief NFA->DFA转换
     *
     * @return DFA 转换后的DFA
     */
    DFA convert() {
        map<set<int>, int> state_map;
        vector<set<int> > dfa_sets;
        auto start_set = set<int>({infa.start});

        queue<set<int> > q;
        q.push(start_set);
        state_map[start_set] = 0;
        dfa_sets.push_back(start_set);

        struct TempState {
            int id;
            bool acc;
            int t0;
            int t1;
        };
        vector<TempState> tmp;

        while (!q.empty()) {
            auto cur = q.front();
            q.pop();
            int cid = state_map[cur];

            bool is_accept = false;
            for (auto s: cur) {
                if (infa.states[s].accept) {
                    is_accept = true;
                    break;
                }
            }

            auto go0 = move_set(cur, '0');
            auto go1 = move_set(cur, '1');

            int go0_id = get_state_id(go0, state_map, dfa_sets, q);
            int go1_id = get_state_id(go1, state_map, dfa_sets, q);

            tmp.push_back({cid, is_accept, go0_id, go1_id});
        }

        if (state_map.find({}) == state_map.end()) {
            int tid = (int) tmp.size();
            state_map[{}] = tid;
            dfa_sets.push_back({});
            tmp.push_back({tid, false, tid, tid});
        }
        int trap_id = state_map[{}];
        for (auto &st: tmp) {
            if (st.t0 < 0) st.t0 = trap_id;
            if (st.t1 < 0) st.t1 = trap_id;
        }

        DFA dfa;
        dfa.states.resize(tmp.size());
        for (auto &st: tmp) {
            dfa.states[st.id].id = st.id;
            dfa.states[st.id].accept = st.acc;
            dfa.states[st.id].t0 = st.t0;
            dfa.states[st.id].t1 = st.t1;
        }
        dfa.start = 0;
        dfa.trap = trap_id;
        return dfa;
    }

private:
    const NFA &infa; ///< 输入NFA引用

    /**
     * @brief 获取子集对应的DFA状态ID
     *
     * @param S NFA状态子集
     * @param m 状态映射表
     * @param d dfa_sets集合
     * @param q 待处理队列
     * @return int DFA状态ID
     */
    int get_state_id(const set<int> &S, map<set<int>, int> &m, vector<set<int> > &d, queue<set<int> > &q) {
        if (S.empty()) return -1;
        if (m.find(S) == m.end()) {
            int id = (int) d.size();
            m[S] = id;
            d.push_back(S);
            q.push(S);
            return id;
        }
        return m[S];
    }

    /**
     * @brief 从NFA状态子集对输入字符c求后继集
     *
     * @param cur 当前NFA状态子集
     * @param c 输入字符('0'或'1')
     * @return set<int> 转移后的状态子集
     */
    set<int> move_set(const set<int> &cur, char c) {
        set<int> res;
        for (auto s: cur) {
            auto it = infa.states[s].trans.find(c);
            if (it != infa.states[s].trans.end()) {
                for (auto nxt: it->second) {
                    res.insert(nxt);
                }
            }
        }
        return res;
    }
};

//-------------------- DFA 最小化 --------------------

/**
 * @brief DFA最小化类
 */
class DFAMinimizer {
public:
    /**
     * @brief 构造函数
     *
     * @param d 待最小化的DFA
     */
    DFAMinimizer(const DFA &d) : idfa(d) {
    }

    /**
     * @brief 最小化DFA
     *
     * @return DFA 最小化后的DFA
     */
    DFA minimize() {
        vector<int> partition(idfa.states.size());
        for (int i = 0; i < (int) idfa.states.size(); i++) {
            partition[i] = idfa.states[i].accept ? 1 : 0;
        }

        bool changed = true;
        while (changed) {
            changed = false;
            map<array<int, 3>, vector<int> > groups;
            for (int i = 0; i < (int) idfa.states.size(); i++) {
                int c0 = partition[idfa.states[i].t0];
                int c1 = partition[idfa.states[i].t1];
                array<int, 3> key = {partition[i], c0, c1};
                groups[key].push_back(i);
            }

            int new_class = 0;
            vector<int> new_partition(idfa.states.size());
            for (auto &g: groups) {
                for (auto s: g.second) {
                    new_partition[s] = new_class;
                }
                new_class++;
            }
            if (new_partition != partition) {
                partition = new_partition;
                changed = true;
            }
        }

        int count_classes = *max_element(partition.begin(), partition.end()) + 1;
        vector<int> repr(count_classes, -1);
        for (int i = 0; i < (int) partition.size(); i++) {
            if (repr[partition[i]] < 0) repr[partition[i]] = i;
        }

        DFA md;
        md.states.resize(count_classes);
        int start_class = partition[idfa.start];
        md.start = start_class;
        int trap_class = partition[idfa.trap];

        for (int c = 0; c < count_classes; c++) {
            int s = repr[c];
            bool class_accept = false;
            for (int i = 0; i < (int) idfa.states.size(); i++) {
                if (partition[i] == c && idfa.states[i].accept) {
                    class_accept = true;
                    break;
                }
            }

            md.states[c].id = c;
            md.states[c].accept = class_accept;
            md.states[c].t0 = partition[idfa.states[s].t0];
            md.states[c].t1 = partition[idfa.states[s].t1];
        }
        md.trap = trap_class;
        return md;
    }

private:
    const DFA &idfa; ///< 输入的未最小化DFA引用
};

//-------------------- TrapRemover 类 --------------------

/**
 * @brief 移除DFA中的陷阱态类
 *
 * 将所有指向陷阱态的转移改为-1，并将陷阱态本身标记无效。
 */
class TrapRemover {
public:
    /**
     * @brief 构造函数
     *
     * @param d DFA引用
     */
    TrapRemover(DFA &d) : dfa(d) {
    }

    /**
     * @brief 移除陷阱态
     */
    void remove_trap() {
        int t = dfa.trap;
        if (t == -1) return;
        for (auto &st: dfa.states) {
            if (st.t0 == t) st.t0 = -1;
            if (st.t1 == t) st.t1 = -1;
        }
        if (t >= 0 && t < (int) dfa.states.size()) {
            dfa.states[t].accept = false;
            dfa.states[t].t0 = -1;
            dfa.states[t].t1 = -1;
        }
        dfa.trap = -1;
    }

private:
    DFA &dfa; ///< DFA引用
};

//-------------------- DFA 输出和RG 转换 --------------------

/**
 * @brief 将最小化DFA打印并转换为右线性文法(RG)输出的类
 */
class DFAPrinter {
public:
    /**
     * @brief 构造函数
     *
     * @param d 最小化且无陷阱态的DFA引用
     */
    DFAPrinter(DFA &d) : idfa(d) {
    }

    /**
     * @brief 打印最小化DFA和对应的RG
     */
    void print_and_convert_to_RG() {
        // 输出最小化DFA
        cout << "      0 1\n";
        vector<string> qname = name_states();
        auto order = get_order(qname);

        for (auto &pr: order) {
            int i = pr.second;
            bool start_mark = (i == idfa.start);
            bool accept_mark = idfa.states[i].accept;
            cout << (start_mark ? "(s)" : "") << (accept_mark ? "(e)" : "") << qname[i] << " ";
            print_transition(idfa.states[i].t0, qname);
            cout << " ";
            print_transition(idfa.states[i].t1, qname);
            cout << "\n";
        }

        cout << "\n"; // 空一行后输出RG

        // 输出RG
        for (auto &p: order) {
            int stid = p.second;
            string lhs = qname[stid];
            vector<string> productions;

            process_symbol(stid, qname, productions);

            for (auto &prod: productions) {
                cout << prod << "\n";
            }
        }
    }

private:
    DFA &idfa; ///< 最小化DFA引用

    /**
     * @brief 为DFA状态命名，如q0,q1,q2...仅为可达状态命名
     *
     * @return vector<string> 每个状态对应的名称
     */
    vector<string> name_states() {
        vector<string> qname(idfa.states.size(), "");
        vector<bool> visited(idfa.states.size(), false);

        if (idfa.start >= 0 && idfa.start < (int) idfa.states.size()) {
            queue<int> Q;
            Q.push(idfa.start);
            visited[idfa.start] = true;
            qname[idfa.start] = "q0";
            int qid_count = 1;

            while (!Q.empty()) {
                int u = Q.front();
                Q.pop();
                int nxts[2] = {idfa.states[u].t0, idfa.states[u].t1};
                for (int i = 0; i < 2; i++) {
                    int v = nxts[i];
                    if (v >= 0 && v < (int) idfa.states.size() && !visited[v]) {
                        visited[v] = true;
                        qname[v] = "q" + to_string(qid_count++);
                        Q.push(v);
                    }
                }
            }
        }
        return qname;
    }

    /**
     * @brief 获取状态按q0,q1,q2,...名称排序的映射
     *
     * @param qname 状态名称数组
     * @return vector<pair<int,int>> (序号,状态ID)的排序结果
     */
    vector<pair<int, int> > get_order(const vector<string> &qname) {
        vector<pair<int, int> > order;
        for (int i = 0; i < (int) qname.size(); i++) {
            if (!qname[i].empty() && qname[i][0] == 'q') {
                int idx = stoi(qname[i].substr(1));
                order.push_back({idx, i});
            }
        }
        sort(order.begin(), order.end());
        return order;
    }

    /**
     * @brief 输出DFA状态转移目标，若无效则输出N
     *
     * @param tid 转移状态ID
     * @param qname 状态名称数组
     */
    void print_transition(int tid, const vector<string> &qname) {
        if (tid == -1 || tid >= (int) qname.size() || qname[tid].empty()) {
            cout << "N";
        } else {
            cout << qname[tid];
        }
    }

    /**
     * @brief 为给定状态输出RG产生式
     *
     * @param stid 状态ID
     * @param qname 状态名称数组
     * @param productions 用于收集产生式的向量
     */
    void process_symbol(int stid, const vector<string> &qname, vector<string> &productions) {
        vector<string> production0;
        for (int c = 0; c < 2; c++) {
            int nxt = (c == 0) ? idfa.states[stid].t0 : idfa.states[stid].t1;
            if (nxt == -1 || nxt >= (int) qname.size() || qname[nxt].empty()) {
                // 无有效转移
                continue;
            }

            string lhs = qname[stid];
            bool next_is_accept = idfa.states[nxt].accept;

            // 若目标状态非纯终结(或有后续转移)，输出qX->c qY
            if (!(next_is_accept && idfa.states[nxt].t0 == -1 && idfa.states[nxt].t1 == -1)) {
                productions.push_back(lhs + "->" + to_string(c) + qname[nxt]);
            }

            // 若目标状态为接受态，也输出qX->c
            if (next_is_accept) {
                production0.push_back(lhs + "->" + to_string(c));
            }
        }
        for (auto &p: production0) {
            productions.push_back(p);
        }
    }
};

//-------------------- main --------------------

/**
 * @brief 主函数
 *
 * 读取正则表达式，从正则到最小化DFA，再到RG的转换并输出。
 */
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string re;
    cin >> re;

    // 1. 解析正则表达式
    RegexParser parser(re);
    RegexNode *root = parser.parse();

    // 2. Thompson构造ε-NFA
    Thompson th(root);
    NFA enfa = th.build();

    // 3. ε-NFA -> NFA
    EpsilonRemover er(enfa);
    NFA nfa = er.remove();

    // 4. NFA -> DFA (子集构造)
    SubsetConstruction sc(nfa);
    DFA dfa = sc.convert();

    // 5. 最小化DFA
    DFAMinimizer dm(dfa);
    DFA mdfa = dm.minimize();

    // 6. 移除DFA中的陷阱态
    TrapRemover tr(mdfa);
    tr.remove_trap();

    // 7. 输出最小化DFA及RG
    DFAPrinter printer(mdfa);
    printer.print_and_convert_to_RG();

    return 0;
}
