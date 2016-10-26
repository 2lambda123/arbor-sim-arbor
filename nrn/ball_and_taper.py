#!/usr/bin/env python
#coding: utf-8

import json
import nrn_validation as V

V.override_defaults_from_args()

# dendrite geometry: 100 µm long, diameter 1 µm to 0.1 µm.
geom = [(0,1), (100, 0.1)]

model = V.VModel()
model.add_soma(12.6157)
model.add_dendrite('taper', geom)
model.add_iclamp(5, 80, 0.3, to='taper')

data = V.run_nrn_sim(100, report_dt=10, model='ball_and_taper')
print json.dumps(data)
V.nrn_stop()

