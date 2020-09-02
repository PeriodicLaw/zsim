#include<iostream>
#include<cstring>
#include<map>
#include<set>
#include<vector>
#include<algorithm>
#include<cassert>

std::map<uint64_t, int> access_count;
std::map<uint64_t, int> bypass_count;
// std::map<uint64_t, int> invalid_replace_count;
std::map<uint64_t, std::map<uint64_t, int>> replace_count;
std::map<uint64_t, std::vector<uint64_t>> replace_count_sorted;
int access_tot, bypass_tot, replace_tot;
std::vector<int> top_tot;

int main(){
    freopen("replace_record.txt","r",stdin);
    char line[100];
    while(std::cin.getline(line, 100)){
        uint64_t addr, replace_addr;
        sscanf(line, "%lx", &addr);
        access_count[addr]++;
        char *p;
        if(p = strstr(line, "yes")){
            sscanf(p+4, "%lx", &replace_addr);
            replace_count[addr][replace_addr]++;
        }else if(strstr(line, "no"))
            bypass_count[addr]++;
        else{
            fprintf(stderr, "error\n");
            return -1;
        }
    }
    fclose(stdin);
    
    freopen("summary.txt","w",stdout);
    for(auto p: access_count){
        auto addr = p.first;
        printf("%lx, %d, %d, %ld, ", addr, access_count[addr], bypass_count[addr], replace_count[addr].size());
        access_tot += access_count[addr];
        bypass_tot += bypass_count[addr];
        
        std::vector<std::pair<int, uint64_t>> top_count;
        for(auto p1: replace_count[addr]){
            replace_tot += p1.second;
            top_count.emplace_back(p1.second, p1.first);
        }
        std::sort(top_count.begin(), top_count.end(), std::greater<std::pair<int, uint64_t>>());
        if(top_tot.size() < top_count.size())
            top_tot.resize(top_count.size());
        
        for(int i=0; i<top_count.size(); i++){
            printf("%lx->%d, ", top_count[i].second, top_count[i].first);
            top_tot[i] += top_count[i].first;
        }
        printf("\n");
    }
    
    freopen("stat.txt","w",stdout);
    printf("bypass: %d\nreplaced: %d\naccess: %d\n", bypass_tot, replace_tot, access_tot);
    printf("bypass/access: %Lf\n", (long double)bypass_tot/access_tot);
    // printf("top k sum/replaced:\n");
    int sum=0;
    for(int i=0; i<top_tot.size(); i++){
        sum += top_tot[i];
        if(i==3) assert((long double)sum/replace_tot > 0.5);
        printf("top%3d sum/replaced: %Lf\n", i+1, (long double)sum/replace_tot);
    }
    
    freopen("simplified.txt","w",stdout);
    for(auto p: access_count){
        auto addr = p.first;
        printf("%lx, %d, %d, %ld, ", addr, access_count[addr], bypass_count[addr], replace_count[addr].size());
        access_tot += access_count[addr];
        bypass_tot += bypass_count[addr];
        
        std::vector<std::pair<int, uint64_t>> top_count;
        for(auto p1: replace_count[addr]){
            replace_tot += p1.second;
            top_count.emplace_back(p1.second, p1.first);
        }
        std::sort(top_count.begin(), top_count.end(), std::greater<std::pair<int, uint64_t>>());
        if(top_tot.size() < top_count.size())
            top_tot.resize(top_count.size());
        
        for(int i=0; i<top_count.size() && i<3; i++){
            printf("%lx->%d, ", top_count[i].second, top_count[i].first);
            top_tot[i] += top_count[i].first;
        }
        if(top_count.size() >= 3){
            auto top1 = top_count[0].second, top2 = top_count[1].second, top3 = top_count[2].second, common = 0ul;
            for(uint64_t x=1; x != 0; x <<= 1){
                int cnt = (bool)(top1 & x) + (bool)(top2 & x) + (bool)(top3 & x);
                if(cnt >= 2)
                    common += x;
            }
            printf("common bits: %lx", common);
        }
        printf("\n");
    }
}