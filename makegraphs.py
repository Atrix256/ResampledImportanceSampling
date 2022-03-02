import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

filenames=[
    "UniformToGaussian",
    "GaussianToXSquared",
    "UniformToGaussianFail",
    "UniformToGaussianFail2"
]

extensions = ["start", "end"]

i = 0
for filename in filenames:
    for extension in extensions:
        df1 = pd.read_csv("./out/" + filename + "." + extension + ".csv")['Value']
        plt.figure(i)
        plt.hist(df1, bins=50)
        plt.title(filename + " " + extension)
        plt.savefig("./out/" + filename + "." + extension + ".png", bbox_inches='tight')
        i = i + 1
