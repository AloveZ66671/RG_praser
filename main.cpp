#include <algorithm>
#include <iostream>
#include <vector>
#include <map>
#include <set>
#include <stack>
#include <queue>
#include <array>
#include <utility>
using namespace std;

static const char EPS = '\0'; ///< 表示ε空转换的特殊字符

//-------------------- Regex Parser --------------------

/**
 * @brief 表示正则表达式的语法树节点类型
 */
struct RegexNode {
    /// 节点类型枚举
    enum Type { CHAR, CONCAT, UNION, STAR } type;
    char ch;                 ///< 对于CHAR类型的节点，表示字符'0'或'1'
    RegexNode *left, *right; ///< 左右子节点，适用于CONCAT、UNION等复合结构

    /**
     * @brief 构造函数
     * @param t 节点类型
     * @param c 对于CHAR类型的字符
     * @param l 左子节点
     * @param r 右子节点
     */
    RegexNode(Type t, char c=0, RegexNode* l=nullptr, RegexNode* r=nullptr)
        : type(t), ch(c), left(l), right(r){}
};

/**
 * @brief 正则表达式解析器，将输入的正则表达式字符串解析为语法树
 */
class RegexParser {
public:
    /**
     * @brief 构造函数
     * @param s 输入的正则表达式字符串
     */
    explicit RegexParser(string s):str(std::move(s)),pos(0){}

    /**
     * @brief 解析整个正则表达式
     * @return 语法树的根节点
     */
    RegexNode* parse() {
        return parseUnion();
    }

private:
    string str; ///< 正则表达式字符串
    int pos;    ///< 当前解析位置

    /**
     * @brief 解析包含'+'的联合(并)运算
     * @return 对应的语法树节点
     */
    RegexNode* parseUnion() {
        RegexNode* node = parseConcat();
        while (pos < static_cast<int>(str.size()) && str[pos] == '+') {
            pos++;
            RegexNode* right = parseConcat();
            node = new RegexNode(RegexNode::UNION,0,node,right);
        }
        return node;
    }

    /**
     * @brief 解析连接运算
     * @return 对应的语法树节点
     */
    RegexNode* parseConcat() {
        RegexNode* node = parseStar();
        while (pos < (int)str.size() && (str[pos] != ')' && str[pos] != '+')) {
            RegexNode* right = parseStar();
            node = new RegexNode(RegexNode::CONCAT,0,node,right);
        }
        return node;
    }

    /**
     * @brief 解析Kleene闭包（*）
     * @return 对应的语法树节点
     */
    RegexNode* parseStar() {
        RegexNode* node = parseBase();
        while (pos < (int)str.size() && str[pos] == '*') {
            pos++;
            node = new RegexNode(RegexNode::STAR,0,node,nullptr);
        }
        return node;
    }

    /**
     * @brief 解析基本单元(字符或括号内的子表达式)
     * @return 对应的语法树节点
     */
    RegexNode* parseBase() {
        if (pos >= (int)str.size()) return nullptr;
        if (str[pos] == '(') {
            pos++;
            RegexNode* node = parseUnion();
            if (pos < (int)str.size() && str[pos] == ')') {
                pos++;
            }
            return node;
        } else if (str[pos] == '0' || str[pos] == '1') {
            RegexNode* node = new RegexNode(RegexNode::CHAR,str[pos]);
            pos++;
            return node;
        }
        return nullptr;
    }
};

//-------------------- NFA定义 --------------------

/**
 * @brief 非确定性有限自动机(NFA)的结构定义
 */
struct NFA {
    /**
     * @brief NFA状态结构
     */
    struct State {
        int id;                             ///< 状态编号
        bool accept = false;                ///< 是否为接受状态
        map<char,vector<int>> trans;        ///< 转移函数，char到状态集合的映射
    };
    vector<State> states;  ///< 状态集合
    int start;             ///< 起始状态ID

    /**
     * @brief 创建新状态
     * @param accept 是否为接受态
     * @return 新状态的ID
     */
    int new_state(bool accept=false) {
        State s;
        s.id = (int)states.size();
        s.accept = accept;
        states.push_back(s);
        return s.id;
    }
};

/**
 * @brief NFA片段，用于Thompson构造法中间过程表示
 */
struct NFAFragment {
    int start;  ///< 片段的起始状态
    int accept; ///< 片段的接受状态
};

/**
 * @brief 使用Thompson构造法将正则表达式语法树转换为ε-NFA
 */
class Thompson {
public:
    /**
     * @brief 构造函数
     * @param root 正则表达式语法树的根节点
     */
    Thompson(RegexNode* root):r(root){}

    /**
     * @brief 构建ε-NFA
     * @return 构建完成的NFA
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
    NFA nfa;       ///< 构造中的NFA

    /**
     * @brief 添加状态转移
     * @param from 源状态
     * @param to 目标状态
     * @param c 转移字符
     */
    void add_transition(int from, int to, char c) {
        nfa.states[from].trans[c].push_back(to);
    }

    /**
     * @brief 针对给定正则节点构建NFA片段
     * @param node 正则节点
     * @return 对应的NFAFragment
     */
    NFAFragment buildFragment(RegexNode* node) {
        switch(node->type) {
            case RegexNode::CHAR: {
                int s = nfa.new_state();
                int a = nfa.new_state();
                add_transition(s,a,node->ch);
                return {s,a};
            }
            case RegexNode::CONCAT: {
                NFAFragment f1 = buildFragment(node->left);
                NFAFragment f2 = buildFragment(node->right);
                add_transition(f1.accept,f2.start,EPS);
                return {f1.start,f2.accept};
            }
            case RegexNode::UNION: {
                int s = nfa.new_state();
                int a = nfa.new_state();
                NFAFragment f1 = buildFragment(node->left);
                NFAFragment f2 = buildFragment(node->right);
                add_transition(s,f1.start,EPS);
                add_transition(s,f2.start,EPS);
                add_transition(f1.accept,a,EPS);
                add_transition(f2.accept,a,EPS);
                return {s,a};
            }
            case RegexNode::STAR: {
                int s = nfa.new_state();
                int a = nfa.new_state();
                NFAFragment f = buildFragment(node->left);
                add_transition(s,f.start,EPS);
                add_transition(f.accept,a,EPS);
                add_transition(s,a,EPS);
                add_transition(f.accept,f.start,EPS);
                return {s,a};
            }
        }
        return {-1,-1};
    }
};

//-------------------- ε-NFA -> NFA --------------------

/**
 * @brief 将ε-NFA转换为无ε转移的NFA
 */
class EpsilonRemover {
public:
    /**
     * @brief 构造函数
     * @param n 输入的ε-NFA
     */
    EpsilonRemover(const NFA &n):infa(n){}

    /**
     * @brief 消除ε转移得到无ε的NFA
     * @return 无ε转移的NFA
     */
    NFA remove() {
        eps_closures.resize(infa.states.size());
        for (int i=0; i<(int)infa.states.size(); i++) {
            compute_epsilon_closure(i);
        }

        NFA onfa;
        onfa.states.resize(infa.states.size());
        for (int i=0; i<(int)infa.states.size(); i++) {
            onfa.states[i].id = i;
        }
        onfa.start = infa.start;

        for (int i=0; i<(int)infa.states.size(); i++) {
            set<int> closure = eps_closures[i];
            bool is_accept = false;
            map<char,set<int>> combined;
            for (int cst: closure) {
                if (infa.states[cst].accept) is_accept = true;
                for (auto &kv: infa.states[cst].trans) {
                    char c = kv.first;
                    if (c==EPS) continue;
                    for (int nxt: kv.second) {
                        for (int ec: eps_closures[nxt]) {
                            combined[c].insert(ec);
                        }
                    }
                }
            }
            onfa.states[i].accept = is_accept;
            for (auto &kv: combined) {
                onfa.states[i].trans[kv.first] = vector<int>(kv.second.begin(),kv.second.end());
            }
        }

        return onfa;
    }

private:
    const NFA &infa;             ///< 输入的ε-NFA
    vector<set<int>> eps_closures; ///< 每个状态的ε闭包

    /**
     * @brief 计算状态s的ε闭包
     * @param s 状态ID
     */
    void compute_epsilon_closure(int s) {
        if (!eps_closures[s].empty()) return;
        stack<int> st;
        st.push(s);
        eps_closures[s].insert(s);
        while(!st.empty()) {
            int u = st.top(); st.pop();
            auto it = infa.states[u].trans.find(EPS);
            if (it!=infa.states[u].trans.end()) {
                for (int nxt: it->second) {
                    if (eps_closures[s].find(nxt)==eps_closures[s].end()) {
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
 * @brief DFA结构定义
 */
struct DFA {
    /**
     * @brief DFA状态结构
     */
    struct State {
        int id;      ///< 状态编号
        bool accept; ///< 是否为接受态
        int t0, t1;  ///< 在输入0或1时的转移状态ID
    };
    vector<State> states; ///< DFA状态集合
    int start;            ///< DFA起始状态ID
    int trap;             ///< 陷阱态ID
    DFA():start(-1),trap(-1){}
};

/**
 * @brief 利用子集构造法将NFA转换为DFA
 */
class SubsetConstruction {
public:
    /**
     * @brief 构造函数
     * @param n 无ε的NFA
     */
    SubsetConstruction(const NFA &n):infa(n){}

    /**
     * @brief 将NFA转换为DFA
     * @return 生成的DFA
     */
    DFA convert() {
        map<set<int>,int> state_map;
        vector<set<int>> dfa_sets;
        auto start_set = set<int>({infa.start});

        queue<set<int>>q;
        q.push(start_set);
        state_map[start_set]=0;
        dfa_sets.push_back(start_set);

        struct TempState{int id;bool acc;int t0;int t1;};
        vector<TempState> tmp;

        while(!q.empty()) {
            auto cur = q.front(); q.pop();
            int cid = state_map[cur];

            bool is_accept = false;
            for (auto s: cur) {
                if (infa.states[s].accept) { is_accept = true; break; }
            }

            auto go0 = move_set(cur,'0');
            auto go1 = move_set(cur,'1');

            int go0_id = get_state_id(go0,state_map,dfa_sets,q);
            int go1_id = get_state_id(go1,state_map,dfa_sets,q);

            tmp.push_back({cid,is_accept,go0_id,go1_id});
        }

        if (state_map.find({})==state_map.end()) {
            int tid = (int)tmp.size();
            state_map[{}]=tid;
            dfa_sets.push_back({});
            tmp.push_back({tid,false,tid,tid});
        }
        int trap_id = state_map[{}];
        for (auto &st: tmp) {
            if (st.t0<0) st.t0 = trap_id;
            if (st.t1<0) st.t1 = trap_id;
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
    const NFA &infa; ///< 输入的NFA

    /**
     * @brief 获取集合S对应的DFA状态ID
     * @param S NFA状态子集
     * @param m 状态映射表
     * @param d dfa_sets集合记录
     * @param q 待处理队列
     * @return DFA状态ID
     */
    int get_state_id(const set<int> &S, map<set<int>,int>&m,vector<set<int>>&d,queue<set<int>>&q) {
        if (S.empty()) return -1;
        if (m.find(S)==m.end()) {
            int id = (int)d.size();
            m[S] = id;
            d.push_back(S);
            q.push(S);
            return id;
        }
        return m[S];
    }

    /**
     * @brief 从当前NFA状态子集对输入字符c求转移的NFA子集
     * @param cur 当前NFA状态子集
     * @param c 输入字符('0'或'1')
     * @return 转移后的NFA状态子集
     */
    set<int> move_set(const set<int>&cur,char c) {
        set<int> res;
        for (auto s: cur) {
            auto it = infa.states[s].trans.find(c);
            if (it!=infa.states[s].trans.end()) {
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
 * @brief 最小化DFA，将其等价类合并并生成最小DFA
 */
class DFAMinimizer {
public:
    /**
     * @brief 构造函数
     * @param d 待最小化的DFA
     */
    DFAMinimizer(const DFA &d):idfa(d){}

    /**
     * @brief 最小化DFA
     * @return 最小化后的DFA
     */
    DFA minimize() {
        vector<int> partition(idfa.states.size());
        for (int i=0; i<(int)idfa.states.size(); i++) {
            partition[i] = idfa.states[i].accept?1:0;
        }

        bool changed = true;
        while(changed) {
            changed = false;
            map<array<int,3>,vector<int>> groups;
            for (int i=0; i<(int)idfa.states.size(); i++) {
                int c0 = partition[idfa.states[i].t0];
                int c1 = partition[idfa.states[i].t1];
                array<int,3> key = {partition[i],c0,c1};
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

        int count_classes = *max_element(partition.begin(), partition.end())+1;
        vector<int> repr(count_classes,-1);
        for (int i=0; i<(int)partition.size(); i++) {
            if (repr[partition[i]]<0) repr[partition[i]]=i;
        }

        DFA md;
        md.states.resize(count_classes);
        int start_class = partition[idfa.start];
        md.start = start_class;
        int trap_class = partition[idfa.trap];

        // 确定每个类的接受态属性
        for (int c=0; c<count_classes; c++) {
            int s = repr[c];
            bool class_accept = false;
            for (int i=0; i<(int)idfa.states.size(); i++) {
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
    const DFA &idfa; ///< 输入的未最小化DFA
};

//-------------------- 输出最小化DFA和RG --------------------

/**
 * @brief 将最小化DFA打印，并转换为右线性文法(RG)输出
 */
class DFAPrinter {
public:
    /**
     * @brief 构造函数
     * @param d 最小化DFA
     */
    DFAPrinter(DFA &d):idfa(d){}

    /**
     * @brief 输出最小化DFA和对应的RG文法
     */
    void print_and_convert_to_RG() {
        cout << "      0 1\n";
        vector<string> qname = name_states();

        // 输出最小化DFA
        auto order = get_order(qname);
        for (auto &pr: order) {
            int i = pr.second;
            bool start_mark = (i==idfa.start);
            bool accept_mark = idfa.states[i].accept;
            cout << (start_mark?"(s)":"") << (accept_mark?"(e)":"") << qname[i] << " "
                 << qname[idfa.states[i].t0] << " " << qname[idfa.states[i].t1] << "\n";
        }

        cout << "\n";

        // 输出RG: 按q0,q1,q2...顺序输出产生式
        // 对于每个状态qX：
        //   qX->0qY
        //   qX->1qZ
        //   qX->0
        //   qX->1
        for (auto &p: order) {
            int stid = p.second;
            string lhs = qname[stid];

            int t0 = idfa.states[stid].t0;
            int t1 = idfa.states[stid].t1;

            cout << lhs << "->0" << qname[t0] << "\n";
            cout << lhs << "->1" << qname[t1] << "\n";
            //如果输入0或1后到达终结态，输出 例如q0->0或q0->1
            if (idfa.states[t0].accept) cout << lhs << "->0\n";
            if (idfa.states[t1].accept) cout << lhs << "->1\n";
        }
    }

private:
    DFA &idfa; ///< 最小化后的DFA引用

    /**
     * @brief 为DFA状态命名(q0, q1, q2...)，使用BFS顺序
     * @return 每个状态对应的名称数组
     */
    vector<string> name_states() {
        vector<string> qname(idfa.states.size(),"");
        vector<bool> visited(idfa.states.size(),false);
        queue<int>Q;
        Q.push(idfa.start);
        visited[idfa.start] = true;
        qname[idfa.start] = "q0";
        int qid_count = 1;
        while(!Q.empty()) {
            int u = Q.front(); Q.pop();
            int nxts[2] = {idfa.states[u].t0, idfa.states[u].t1};
            for (int i=0;i<2;i++){
                int v = nxts[i];
                if (!visited[v]) {
                    visited[v]=true;
                    qname[v]="q"+to_string(qid_count++);
                    Q.push(v);
                }
            }
        }
        // 对未访问状态(如陷阱态)命名
        for (int i=0; i<(int)idfa.states.size(); i++) {
            if (qname[i].empty()) {
                qname[i] = "q" + to_string(qid_count++);
            }
        }
        return qname;
    }

    /**
     * @brief 获取状态按名称(q0,q1,q2...)排序的映射
     * @param qname 状态名称数组
     * @return (序号,状态ID)的排序结果
     */
    vector<pair<int,int>> get_order(const vector<string> &qname) {
        vector<pair<int,int>> order;
        for (int i=0; i<(int)qname.size(); i++) {
            if (!qname[i].empty() && qname[i][0]=='q') {
                int idx = stoi(qname[i].substr(1));
                order.push_back({idx,i});
            }
        }
        sort(order.begin(),order.end());
        return order;
    }
};

//-------------------- main --------------------

/**
 * @brief 主函数，执行正则表达式->最小化DFA->RG转换的完整流程
 */
int main() {
    ios::sync_with_stdio(false);
    cin.tie(nullptr);

    string re;
    cin >> re;

    // 1. 解析正则表达式
    RegexParser parser(re);
    RegexNode* root = parser.parse();

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

    // 6. 输出最小化DFA及RG
    DFAPrinter printer(mdfa);
    printer.print_and_convert_to_RG();

    return 0;
}
