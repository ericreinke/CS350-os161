#!/bin/bash

if [ "$1" == "" ]; then
	   echo 'Require args'
   else
	      echo 'Building OS161'
	         cd ~/cs350-os161/os161-1.99/kern/compile
		    cd ASST$1
		       bmake depend
		          bmake
			     bmake install

			        if [ "$2" == "-r" ]; then
					       cd ~/cs350-os161/root
					              echo '@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@'
						             sys161 kernel-ASST$1
							       fi
							fi
