# CHATS Artifact instructions
In this document we explain how to install and deploy containers for MICRO submission 521 Chaining Transactions for Effective Concurrency Management in Hardware Transactional Memory. This application contains the gem5 simulator used for the experiments, a linux image with the benchmark binaries and inputs, scripts prepared to reproduce the results and to analyze the data.

## Requirements
- KVM must be enabled in your machine.
- Docker ver. 27.1.1
- Docker compose ver. 2.29.1
- Ubuntu (for this experiments ver. 20.04 LTS was used)
- Virtualization support (Check in your BIOS)
- R ver. 4.4.1
For ease of use and reduced reproducibility complexity, we provided the user with a make file that is automated to easily install almost all these tools.

## Deploy steps
1. Execute make build at the root of the repository to prepare all the configurations and containers. This step can take a while since the simulator is big and it will need to build from scratch. Additionally, root permission will be required since it will have to install several dependencies, reduce perf paranoid from kernel, use docker, etc.
~~~bash
sudo make build
~~~
2. Once the application is fully built, it will prompt with the message "Everything built correctly!". Additionally, will print some notes that the user should carefully read. It is now time to run the containers. One objective is prepared to compose and run all of them. While building, the application automatically resolved how many containers would create depending on your system specifications. Again, as docker is being run, use sudo.
~~~bash
sudo make run
~~~
3. If there is no problem or issue with docker (hopefully), the message "Containers running!" will prompt. This signal that all containers are running without problem, additionally, it will print all the containers that are started to the user.
Now to connect with the slurm controller node, execute this command and it will connect to the container with root user.
~~~bash
sudo make connect
~~~
4. What this connect command is really doing is to start a bash session with a terminal connection to the container, which is only running slurm controller daemon. Additionally, a binding is performed with the containers/results folder. This binding is performed with the /gem5/results folder, so whatever is done here, it is shared with the host machine.
Inside the gem5 folder, there is a directory called gem5_path that contains all required to run full-system simulations and some scripts that can help the user in many use cases. We created a folder with all the scripts that submit the simulations that give the results shown in the paper. To replicate, for example, the results from the main figures, you can execute the following commands:
~~~bash
cd /gem5/gem5_path/scripts/2023-02.valuepred/
./generate-simulations-main --enqueue
~~~
This command will generate a bunch of folders containing the simulation scripts that are executed in batches by slurm. You will be able to find the results from the execution in stats.txt file, along with the simulation configuration used. For example, once the simulation finishes, you can perform the next operations to access the stats file:
~~~bash
cd /gem5/results/valuepred-main/CPUtest_BinSfx.htm.fallbacklock_LV_ED_CRrw_RSL0Ev_RSPrec_L0Repl_L1Repl_RldStale_DwnG_Rtry6_Pflt/stamp.genome/0
less stats.txt
~~~
5. You can check if everything is working by using slurm:
~~~bash
squeue
~~~
With this command you can observe the current status of every work submitted to slurm and which node (container) is executing the batch.
Additionally, we implemented another script that can be used to check the current status of every execution:
~~~bash
/gem5/gem5_path/scripts/check_simulations.sh /gem5/results
~~~
This command will show every simulation status in a recurrent fashion from the /gem5/results folder. The first row will show either if the simulation did not start yet with NOSTDOUT, how many work units are performed with R N or if the simulation finished with COMPLETED_RoI.
6. As last point, you probably will want to close all the containers. We included an objective with make that do everything for the user.
~~~bash
sudo make clean
~~~
Nevertheless, for unknown reasons for us, docker seems to allocate space in disk for every build of a layer in the dockerfile. Additionally, docker prune does not seem to clean all these even if -a option is executed (which is the alias for the objective from makefile). We removed all these files by removing /var/lib/docker folder as suggested in: https://stackoverflow.com/questions/46672001/is-it-safe-to-clean-docker-overlay2.
~~~bash
# NOTE: Executing this command will remove all images and containers from your docker environment. BE CAUTIOUS and acknowledge this fact
sudo -s
systemctl stop docker
rm -rf /var/lib/docker
systemctl start docker
~~~
## Getting the results
To obtain the results we used [RING-5](https://github.com/nikiitin/RING-5.git) which is a R-based statistical analysis tool for gem5. Note that even if it is public it is an experimental tool and is prone to failure. We pushed a specific branch (MICRO24) that will be kept to keep reproducibility. This tool will be built and configured altogether with the previous ```make build``` command.
Once all the simulations are finished, you can execute the following command that will execute RING-5 with the configurations for this project:
~~~bash
make plot
~~~
It will try to execute RING-5 for all the stats files found at ```results``` folder. It will create several folders, each containing the results of the experiments, both in csv and in graphic format with png files and each corresponding to the figure in the original manuscript.