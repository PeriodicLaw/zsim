# cases="cassandra"
cases="cassandra drupal finagle-chirper finagle-http kafka mediawiki tomcat verilator wordpress"
for case in $cases; do
    echo ""
    echo "testing "$case"...."
    echo ""
    
    cat tests/clang.cfg | awk '{ gsub(/{case}/,"'$case'"); gsub(/{repltype}/,"GuidedLRU"); print $0 }' > tests/clang_tmp.cfg
    build/opt/zsim_trace tests/clang_tmp.cfg || exit -1
    cp zsim.out zsimout/$case/opt_guided_lru.out
    
    # mkdir -p zsimout/$case
    # cp tests/traces/$case/zsim_lru1.out zsimout/$case/lru.out
    # cp tests/traces/$case/zsim_opt1.out zsimout/$case/opt.out
    
    # cat tests/clang.cfg | awk '{ gsub(/{case}/,"'$case'"); gsub(/{repltype}/,"LRUBypass"); print $0 }' > tests/clang_tmp.cfg
    # build/opt/zsim_trace tests/clang_tmp.cfg || exit -1
    # # exit
    # cp zsim.out zsimout/$case/lru_bypass.out
    
    # cat tests/clang.cfg | awk '{ gsub(/{case}/,"'$case'"); gsub(/{repltype}/,"OptBypass"); print $0 }' > tests/clang_tmp.cfg
    # build/opt/zsim_trace tests/clang_tmp.cfg || exit -1
    # cp zsim.out zsimout/$case/opt_bypass.out
    
    # cat tests/clang.cfg | awk '{ gsub(/{case}/,"'$case'"); gsub(/{repltype}/,"GHRP"); print $0 }' > tests/clang_tmp.cfg
    # build/opt/zsim_trace tests/clang_tmp.cfg || exit -1
    # cp zsim.out zsimout/$case/ghrp.out
    
    # mkdir -p opt_record/$case
    # cat tests/clang.cfg | awk '{ gsub(/{case}/,"'$case'"); gsub(/{repltype}/,"OptBypass"); print $0 }' > tests/clang_tmp.cfg
    # build/opt/zsim_trace tests/clang_tmp.cfg || exit -1
    # cd opt_record/$case
    # ../../replace_record_process
    # cd -
    
    #cat tests/clang.cfg | awk '{ gsub(/{case}/,"'$case'"); gsub(/{record}/,"false"); gsub(/{repltype}/,"Opt"); print $0 }' > tests/clang_tmp.cfg
    #build/opt/zsim_trace tests/clang_tmp.cfg || exit -1
    #cp zsim.out "tests/traces/"$case"/zsim_opt_fullassoc.out"
    
done
rm tests/clang_tmp.cfg