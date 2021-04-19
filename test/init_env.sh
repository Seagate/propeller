# SPDX-License-Identifier: LGPL-2.1-only
# Copyright (C) 2021 Seagate Technology LLC and/or its Affiliates.

# Setup the environment for testing ILM.
PARENT_DIR="$(dirname "$(pwd)")"

# Use built libraries from source
export LD_LIBRARY_PATH="$PARENT_DIR"/src

# Run directory for ILM
export ILM_RUN_DIR=/tmp/seagate_ilm/

# Import python extension module from source.
export PYTHONPATH="$PARENT_DIR"/python
