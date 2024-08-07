import argparse
from common import ObjectList
import m5.objects
from m5.objects.BranchPredictor import *
from m5.objects.O3CPU import *
from m5.params import *

class CpuParam(argparse.Action):
    # Constructor, use it to set the values of the parameters
    # This action will be called when the option is found
    def __call__(self, parser, namespace, values, option_string=None):
        # Set the values of the parameters
        setattr(namespace, self.dest, dict())
        # Remove the values "[]" from the string
        value = values.replace("[", "")
        value = value.replace("]", "")
        # Split the string for every comma
        value = value.split(",")
        # remove the spaces from the beginning and the end of the string
        value = [elem.strip() for elem in value]
        # Remove the quotes from the beginning and the end of the string
        value = [elem.strip("\'") for elem in value]
        # Now we have a list of params as name=value, split it by "=", and
        # assign it to the dictionary
        for conf in value:
            confName, confVal = conf.split("=", 1)
            # Assign it to the dictionary
            if confName == "branchPred":
                # Special case for branch predictors
                # The call to constructor of the branch predictor
                # is in the value of the parameter
                # execute the constructor and assign the result
                # If no branch predictor is found, there will be an exception
                # and the program will exit
                getattr(namespace, self.dest)[confName] = confVal
            else:
                getattr(namespace, self.dest)[confName] = confVal
