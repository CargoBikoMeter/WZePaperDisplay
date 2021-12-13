#!/bin/bash

# Copyright (C) 2021 WZePaperDisplay Contributors
# Contact: SAI-Lab Berlin (https://www.chemie.tu-berlin.de/sai_lab/sei_real_labor_sai_lab)
# 
# This file is part of the WZePaperDisplay firmware.
#
# The WZePaperDisplay firmware is free software: you can
# redistribute it and/or modify it under the terms of the GNU
# Lesser General Public License as published by the Free Software
# Foundation, either version 3 of the License, or (at your option)
# any later version.

# WZePaperDisplay firmware is distributed in the hope that
# it will be useful, but WITHOUT ANY WARRANTY; without even the
# implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
# PURPOSE. See the GNU Lesser General Public License for more
# details.

# You should have received a copy of the GNU Lesser General Public
# License along with the WZePaperDisplay firmware. If not,
# see <http://www.gnu.org/licenses/>.

#  Based on https://github.com/openbikesensor/OpenBikeSensorFirmware/
#
#  The OpenBikeSensor firmware is free software: you can redistribute it
#  and/or modify it under the terms of the GNU Lesser General Public License as
#  published by the Free Software Foundation, either version 3 of the License,
#  or (at your option) any later version.
#
#  OpenBikeSensor firmware is distributed in the hope that it will be
#  useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser
#  General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public License
#  along with the OpenBikeSensor firmware.  If not, see
#  <http://www.gnu.org/licenses/>.
#

# based on https://raw.githubusercontent.com/fhessel/esp32_https_server/master/extras/create_cert.sh
# Certs must be updated from time to time,
# might be once we get a proper update mechanism
# in place. For now it is a static - all the same
# cert.
# Its sole purpose is to allow encrypted communication between the OBS
# and a local browser.

set -e
#------------------------------------------------------------------------------
# cleanup any previously created files
rm -f wzca.* wz.* cert.h private_key.h

#------------------------------------------------------------------------------
# create a CA called "myca"

# create a private key
openssl genrsa -out wzca.key 1024

# create certificate
cat > wzca.conf << EOF
[ req ]
distinguished_name     = req_distinguished_name
prompt                 = no
[ req_distinguished_name ]
C = DE
L = Berlin
O = adfc-tk.de 
CN = wz-ca.local
EOF
openssl req -new -x509 -days 3650 -key wzca.key -out wzca.crt -config wzca.conf
# create serial number file
echo "01" > wzca.srl

#------------------------------------------------------------------------------
# create a certificate for the ESP (hostname: "WZePaperDisplay")

# create a private key
openssl genrsa -out wz.key 1024
# create certificate signing request
cat > wz.conf << EOF
[ req ]
distinguished_name     = req_distinguished_name
prompt                 = no
[ req_distinguished_name ]
C = DE
L = Berlin
O = adfc-tk.de 
CN = wz.local
EOF
openssl req -new -key wz.key -out wz.csr -config wz.conf

# have myca sign the certificate
openssl x509 -days 3650 -CA wzca.crt -CAkey wzca.key -in wz.csr -req -out wz.crt

# verify
openssl verify -CAfile wzca.crt wz.crt

# convert private key and certificate into DER format
openssl rsa -in wz.key -outform DER -out wz.key.DER
openssl x509 -in wz.crt -outform DER -out wz.crt.DER

# create header files
echo "#ifndef CERT_H_" > ./cert.h
echo "#define CERT_H_" >> ./cert.h
xxd -i wz.crt.DER >> ./cert.h
echo "#endif" >> ./cert.h

echo "#ifndef PRIVATE_KEY_H_" > ./private_key.h
echo "#define PRIVATE_KEY_H_" >> ./private_key.h
xxd -i wz.key.DER >> ./private_key.h
echo "#endif" >> ./private_key.h

echo ""
echo "Certificates created!"
echo "---------------------"
echo ""
echo "  Private key:      private_key.h"
echo "  Certificate data: cert.h"
echo ""
echo "Make sure to have both files available for inclusion when running the examples."
echo "The files have been copied to all example directories, so if you open an example"
echo " sketch, you should be fine."
