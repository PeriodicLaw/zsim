export PINPATH=/home/perl/work/nsy/pin-3.13-98189-g60a6ef199-gcc-linux
export XEDPATH=/home/perl/work/nsy/xed
export LD_LIBRARY_PATH=$XEDPATH/kits/xed-install-base-2019-11-27-lin-x86-64/lib/
export DRIOPATH=/home/perl/work/nsy/dynamorio
source temp-python/bin/activate
alias run="build/opt/zsim_trace tests/clang.cfg"
alias predbg="rm core; ulimit -c unlimited"
alias postdbg="gdb build/opt/zsim_trace core"

#export PINPATH=/home/vagrant/pin-2.14-71313-gcc.4.4.7-linux
#export XEDPATH=/vagrant/xed
#export LD_LIBRARY_PATH=$XEDPATH/kits/xed-install-base-2019-11-27-lin-x86-64/lib/
#export DRIOPATH=/vagrant/dynamoriosc
