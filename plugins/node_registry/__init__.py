# Copyright 2019 Luma Pictures
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
import _nodeRegistryArnold
from pxr import Tf

Tf.PrepareModule(_nodeRegistryArnold, locals())
del Tf

try:
    import __DOC

    __DOC.Execute(locals())
    del __DOC
except Exception:
    try:
        import __tmpDoc

        __tmpDoc.Execute(locals())
        del __tmpDoc
    except:
        pass
