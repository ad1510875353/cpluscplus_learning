#include <iostream>
#include <vector>
#include <unordered_set>
using namespace std;


int main()
{   
    string s,t;
    cin >> s;
    cin >> t;
    // 记录i位置之前的单词种类数量
    vector<int> snum(s.size());
    vector<int> tnum(s.size());
    unordered_set<char> sset;
    unordered_set<char> tset;

    for(int i = 0; i < s.size(); i++){
        sset.insert(s[i]);
        tset.insert(t[i]);
        snum[i] = sset.size();
        tnum[i] = tset.size();
    }
    int ans = 0;
    vector<vector<int>> ops;
    for(int i = s.size()-1; i>=0; i--){
        if(s[i]!=t[i]){
            ans++;

            if(snum[i] > tnum[i]){
                vector<int> op = {1,i+1,t[i]};
                ops.push_back(op);
                for(int j = 0; j <= i; j++){
                    snum[j] = 1;
                    s[j] = t[i];
                }
            }else{
                vector<int> op = {2,i+1,s[i]};
                ops.push_back(op);
                for(int j = 0; j <= i; j++){
                    tnum[j] = 1;
                    t[j] = s[i];
                }
            }
        }
    }
    cout << ans << endl;
    for(int i = 0 ; i < ans; i++)
        printf("%d %d %c \n",ops[i][0],ops[i][1],ops[i][2]);

    return 0;
}