#!/usr/bin/env bash
cd ~/rq-bench || exit 1
OUT=bench100.txt; : > "$OUT"
for g in 1 2 3 4 5; do
  for i in $(seq 20); do
    taskset -c 0,2 ./ring_bench | awk 'NR==3{print $2+0}' >> "$OUT"
  done
  echo "group $g done"
  [ "$g" -lt 5 ] && sleep 60
done
echo "---- 统计 ----"
awk '{v=$1;n++;s+=v;if(n==1||v<mn)mn=v;if(v>mx)mx=v} END{printf "n=%d  min=%.2f  max=%.2f  avg=%.2f  Mops\n",n,mn,mx,s/n}' "$OUT"
