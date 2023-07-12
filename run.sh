#!/bin/bash
cd $HOME/ext/hidpp/build/src/tools

echo -n "Finding device..."

devid=$( ./hidpp-list-devices 2>/dev/null | egrep 'dev://[0-9]+:' | awk -F: '{print $2}' )

echo "ok"

# ./hidpp-list-features dev://4296645382 -d 1 -v

soc() {
[ $1 -gt 4186 ] && echo     100%	&& return				
[ $1 -gt 4156 ] && echo 	99%		&& return			
[ $1 -gt 4143 ] && echo 	98%		&& return			
[ $1 -gt 4133 ] && echo 	97%		&& return			
[ $1 -gt 4122 ] && echo 	96%		&& return			
[ $1 -gt 4113 ] && echo 	95%		&& return			
[ $1 -gt 4103 ] && echo 	94%		&& return			
[ $1 -gt 4094 ] && echo 	93%		&& return			
[ $1 -gt 4086 ] && echo 	92%		&& return			
[ $1 -gt 4076 ] && echo 	91%		&& return			
[ $1 -gt 4067 ] && echo 	90%		&& return			
[ $1 -gt 4060 ] && echo     89%		&& return			
[ $1 -gt 4051 ] && echo 	88%		&& return			
[ $1 -gt 4043 ] && echo 	87%		&& return			
[ $1 -gt 4036 ] && echo 	86%		&& return			
[ $1 -gt 4027 ] && echo 	85%		&& return			
[ $1 -gt 4019 ] && echo 	84%		&& return			
[ $1 -gt 4012 ] && echo 	83%		&& return			
[ $1 -gt 4004 ] && echo 	82%		&& return			
[ $1 -gt 3997 ] && echo 	81%		&& return			
[ $1 -gt 3989 ] && echo 	80%		&& return			
[ $1 -gt 3983 ] && echo 	79%		&& return			
[ $1 -gt 3976 ] && echo 	78%		&& return			
[ $1 -gt 3969 ] && echo 	77%		&& return			
[ $1 -gt 3961 ] && echo 	76%		&& return			
[ $1 -gt 3955 ] && echo 	75%		&& return			
[ $1 -gt 3949 ] && echo 	74%		&& return			
[ $1 -gt 3942 ] && echo 	73%		&& return			
[ $1 -gt 3935 ] && echo 	72%		&& return			
[ $1 -gt 3929 ] && echo 	71%		&& return			
[ $1 -gt 3922 ] && echo 	70%		&& return			
[ $1 -gt 3916 ] && echo 	69%		&& return			
[ $1 -gt 3909 ] && echo 	68%		&& return			
[ $1 -gt 3902 ] && echo 	67%		&& return			
[ $1 -gt 3896 ] && echo 	66%		&& return			
[ $1 -gt 3890 ] && echo     65%		&& return			
[ $1 -gt 3883 ] && echo 	64%		&& return			
[ $1 -gt 3877 ] && echo 	63%		&& return			
[ $1 -gt 3870 ] && echo     62%		&& return			
[ $1 -gt 3865 ] && echo 	61%		&& return			
[ $1 -gt 3859 ] && echo 	60%		&& return			
[ $1 -gt 3853 ] && echo 	59%		&& return			
[ $1 -gt 3848 ] && echo 	58%		&& return			
[ $1 -gt 3842 ] && echo 	57%		&& return			
[ $1 -gt 3837 ] && echo 	56%		&& return			
[ $1 -gt 3833 ] && echo 	55%		&& return			
[ $1 -gt 3828 ] && echo 	54%		&& return			
[ $1 -gt 3824 ] && echo 	53%		&& return			
[ $1 -gt 3819 ] && echo 	52%		&& return			
[ $1 -gt 3815 ] && echo 	51%		&& return			
[ $1 -gt 3811 ] && echo 	50%		&& return			
[ $1 -gt 3808 ] && echo 	49%		&& return			
[ $1 -gt 3804 ] && echo 	48%		&& return			
[ $1 -gt 3800 ] && echo 	47%		&& return			
[ $1 -gt 3797 ] && echo 	46%		&& return			
[ $1 -gt 3793 ] && echo 	45%		&& return			
[ $1 -gt 3790 ] && echo 	44%		&& return			
[ $1 -gt 3787 ] && echo 	43%		&& return			
[ $1 -gt 3784 ] && echo 	42%		&& return			
[ $1 -gt 3781 ] && echo 	41%		&& return			
[ $1 -gt 3778 ] && echo 	40%		&& return			
[ $1 -gt 3775 ] && echo 	39%		&& return			
[ $1 -gt 3772 ] && echo 	38%		&& return			
[ $1 -gt 3770 ] && echo 	37%		&& return			
[ $1 -gt 3767 ] && echo 	36%		&& return			
[ $1 -gt 3764 ] && echo 	35%		&& return			
[ $1 -gt 3762 ] && echo 	34%		&& return			
[ $1 -gt 3759 ] && echo 	33%		&& return			
[ $1 -gt 3757 ] && echo 	32%		&& return			
[ $1 -gt 3754 ] && echo 	31%		&& return			
[ $1 -gt 3751 ] && echo 	30%		&& return			
[ $1 -gt 3748 ] && echo 	29%		&& return			
[ $1 -gt 3744 ] && echo 	28%		&& return			
[ $1 -gt 3741 ] && echo 	27%		&& return			
[ $1 -gt 3737 ] && echo 	26%		&& return			
[ $1 -gt 3734 ] && echo 	25%		&& return			
[ $1 -gt 3730 ] && echo 	24%		&& return			
[ $1 -gt 3726 ] && echo 	23%		&& return			
[ $1 -gt 3724 ] && echo 	22%		&& return			
[ $1 -gt 3720 ] && echo 	21%		&& return			
[ $1 -gt 3717 ] && echo 	20%		&& return			
[ $1 -gt 3714 ] && echo 	19%		&& return			
[ $1 -gt 3710 ] && echo 	18%		&& return			
[ $1 -gt 3706 ] && echo 	17%		&& return			
[ $1 -gt 3702 ] && echo 	16%		&& return			
[ $1 -gt 3697 ] && echo 	15%		&& return			
[ $1 -gt 3693 ] && echo 	14%		&& return			
[ $1 -gt 3688 ] && echo 	13%		&& return			
[ $1 -gt 3683 ] && echo 	12%		&& return			
[ $1 -gt 3677 ] && echo 	11%		&& return			
[ $1 -gt 3671 ] && echo 	10%		&& return			
[ $1 -gt 3666 ] && echo 	9%		&& return			
[ $1 -gt 3662 ] && echo 	8%		&& return			
[ $1 -gt 3658 ] && echo 	7%		&& return			
[ $1 -gt 3654 ] && echo 	6%		&& return			
[ $1 -gt 3646 ] && echo 	5%		&& return			
[ $1 -gt 3633 ] && echo 	4%		&& return			
[ $1 -gt 3612 ] && echo 	3%		&& return			
[ $1 -gt 3579 ] && echo 	2%		&& return			
[ $1 -gt 3537 ] && echo 	1%		&& return			
[ $1 -gt 3500 ] && echo 	0%		&& return
}

conv() {
  [ "$1$2" = "" ] && return
  printf "=== volts: %d\n" "0x$1$2"
  soc $( printf "%d" "0x$1$2" )
}

#conv $( ./hidpp20-call-function dev://4295916646 -d 1 4 0 )
#conv $( ./hidpp20-call-function dev://4296645382 -d 1 4 0 )
#conv $( ./hidpp20-call-function dev://4296035377 -d 1 4 0 )
#conv $( ./hidpp20-call-function dev://4295544670 -d 1 4 0 )
conv $( ./hidpp20-call-function dev:$devid -d 1 4 0 2>/dev/null )
conv $( ./hidpp20-call-function dev:$devid -d 1 6 0 2>/dev/null )
