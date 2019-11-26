#! /usr/bin/python3

# This is the real nest program, which requires NESTIO + ARBOR-NESTIO

from sys import argv
argv.append('--quiet')
import sys

print("Getting comm")
from mpi4py import MPI
comm = MPI.COMM_WORLD.Split(0) # is nest

print("Getting nest")
import nest
nest.set_communicator(comm)
nest.SetKernelStatus({'recording_backends': {'arbor':{}}})

print("Building network")
pg = nest.Create('poisson_generator', params={'rate': 10.0})
parrots = nest.Create('parrot_neuron', 100)
nest.Connect(pg, parrots)

# sd = nest.Create('spike_detector',
                 # params={"record_to": "screen"})
# nest.Connect(parrots, sd)

sd2 = nest.Create('spike_detector',
                  params={"record_to": "arbor"})
				  
nest.Connect(parrots, sd2)

#print(nest.GetKernelStatus())

#nest.SetKernelStatus({'recording_backends': {'screen': {}}})
status = nest.GetKernelStatus()
print('min_delay: ', status['min_delay'], ", max_delay: ", status['max_delay'])
print( "debug 1*******************" + str( status['network_size'] ) + "*************")

#nest.ResetKernel()
#nest.SetKernelStatus({'min_delay': status['min_delay']/2,
#                      'max_delay': status['max_delay']})
status = nest.GetKernelStatus()
print('min_delay: ', status['min_delay'], ", max_delay: ", status['max_delay'])
print( "debug 2*******************" + str( status['network_size'] ) + "*************")
print("Simulate")
sys.stdout.flush()
#nest.ResetKernel()
nest.Simulate(100.0)
print("Done")
