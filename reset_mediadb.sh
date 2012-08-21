#!/bin/sh

if [ $# -ne 1 ]
then
	echo 'Usage : reset_mediadb.sh [phone|sd|all]'
	exit
fi

if [ $1 = "phone" ]
then
	vconftool set -f -t int db/filemanager/dbupdate "0"
	killall media-server
fi

if [ $1 = "sd" ]
then
	vconftool set -f -t string db/private/mediaserver/mmc_info ""
	killall media-server
fi

if [ $1 = "all" ]
then
	vconftool set -f -t int db/filemanager/dbupdate "0"
	vconftool set -f -t string db/private/mediaserver/mmc_info ""
	killall media-server
fi


