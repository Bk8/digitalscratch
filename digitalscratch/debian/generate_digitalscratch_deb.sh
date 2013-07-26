#!/bin/bash

################################################################################
# Generate ubuntu packages: digitalscratch
################################################################################

# Error checking
function check_error {
    if [ $? -gt 0 ]; then
        echo "ERROR ! ABORTING !"
		exit
	fi
}

# Usage
function usage {
    echo ""
    echo "Usage: generate_digitalscratch_deb.sh [version_number]"
    echo ""
    echo "    [ppa_type]       'test' (for the test PPA url) or 'prod'"
    echo ""
    exit
}

# Check parameters
if [ $# -ne 1 ]; then
    usage
fi

# Select PPA
if [[ $1 == test ]] ; then
    PPAPATH=julien-rosener/digitalscratch-test
    PPAPATH_LIBKEYFINDER=julien-rosener/keyfinder-test
elif [[ $1 == prod ]] ; then
    PPAPATH=julien-rosener/digitalscratch
    PPAPATH_LIBKEYFINDER=julien-rosener/keyfinder
else
    usage
fi

echo "****************************** Install tools ****************************"
sudo apt-get install packaging-dev build-essential dh-make
check_error
echo ""
echo ""

echo "************************* Get version from .pro ************************"
VERSION=$(cat ../digitalscratch.pro | grep 'VERSION =' | cut -d'=' -f2 | tr -d ' ')
echo VERSION = $VERSION
check_error
echo ""
echo ""

echo "*************************** Prepare environment *************************"
# Main vars
VERSIONPACKAGE=$VERSION-0ubuntu1
WORKINGPATH=$HOME/digitalscratch_$VERSION-make_package
DEBPATH=$WORKINGPATH/deb
SOURCEDIR=digitalscratch_source
TARPACK=digitalscratch_$VERSION.orig.tar.gz
ORIGDIR=$(pwd)
DISTRIB=$(lsb_release -cs)
PPAURL=http://ppa.launchpad.net
export DEBEMAIL=julien.rosener@digital-scratch.org
export DEBFULLNAME="Julien Rosener"
export EDITOR=vim

rm -rf $WORKINGPATH
check_error
mkdir -v $WORKINGPATH
check_error
echo ""
echo ""

echo "**************************** Copy source code ***************************"
git checkout changelog
check_error
cd ..
git archive --format zip --output $WORKINGPATH/archive.zip master
unzip $WORKINGPATH/archive.zip -d $WORKINGPATH/$SOURCEDIR
check_error
cd debian
echo ""
echo ""

echo "************************* Update changelog ******************************"
ORIGDIR=$(pwd)
cd $WORKINGPATH/$SOURCEDIR
debchange --newversion $VERSIONPACKAGE --distribution $DISTRIB
check_error
cat $WORKINGPATH/$SOURCEDIR/debian/changelog
cp $WORKINGPATH/$SOURCEDIR/debian/changelog $ORIGDIR
check_error
echo ""
echo ""

echo "************************* Compress source directory *********************"
cd $WORKINGPATH
tar cvzf $TARPACK $SOURCEDIR/
echo ""
echo ""

echo "***************************** Create Linux base *************************"
export BUILDUSERID=$USER
cd $WORKINGPATH/$SOURCEDIR
if [ ! -f ~/pbuilder/$DISTRIB-base.tgz ]
then
    pbuilder-dist $DISTRIB create
fi
export OTHERMIRROR="deb $PPAURL/$PPAPATH/ubuntu $DISTRIB main; deb $PPAURL/$PPAPATH_LIBKEYFINDER/ubuntu $DISTRIB main"
pbuilder-dist $DISTRIB update
echo ""
echo ""

echo "************Parse debian/ config file and create source.changes *********"
debuild -S -sa
check_error
cd ../
echo ""
echo ""

echo "***************************** Create test DEB files *********************"
mkdir -v $DEBPATH
pbuilder-dist $DISTRIB build --allow-untrusted --buildresult $DEBPATH *.dsc
check_error
echo ""
echo ""

echo "************ Upload source.changes on Launchpad at ppa:$PPAPATH *************"
dput -f ppa:$PPAPATH *source.changes
check_error
echo ""
echo ""

echo "    Done, testing DEBs are in $DEBPATH"
