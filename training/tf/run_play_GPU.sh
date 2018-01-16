#!/bin/bash -l
#SBATCH --job-name=fivemok_play
#SBATCH --nodes=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=16G
#SBATCH --partition=GPU
#SBATCH --gres=gpu:1
#SBATCH --workdir=/home2/s175443/work/image_analysis/leela-zero/autogtp
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test &
/home2/s175443/work/image_analysis/leela-zero/autogtp/autogtp -k test 
