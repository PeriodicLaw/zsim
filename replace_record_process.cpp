#include<iostream>
#include<map>
#include<set>
#include<cstring>
std::map<uint64_t, int> access_count;
std::map<uint64_t, int> bypass_count;
// std::map<uint64_t, int> invalid_replace_count;
std::map<uint64_t, std::map<uint64_t, int>> replace_count;

int main(){
    freopen("replace_record.txt","r",stdin);
    freopen("replace_record_summary.txt","w",stdout);
    
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
    
    for(auto p: access_count){
        auto addr = p.first;
        printf("%lx, %d, %d, %ld, ", addr, access_count[addr], bypass_count[addr], replace_count[addr].size());
        for(auto p1: replace_count[addr])
            printf("%lx->%d, ", p1.first, p1.second);
        std::cout << std::endl;
    }
}