import matplotlib.pyplot as plt
import numpy as np
import pandas as pd

filenames=[
    "UniformToGaussian",
    "GaussianToXSquared",
    "UniformToGaussianFail",
    "UniformToGaussianFail2"
]

for filename in filenames:
    fig, (ax1, ax2) = plt.subplots(1, 2)
    fig.suptitle(filename + " Histogram")
    df1 = pd.read_csv("./out/" + filename + ".start.csv")
    df2 = pd.read_csv("./out/" + filename + ".end.csv")
    df1 = df1.set_index('Value')
    df2 = df2.set_index('Value')
    ax1.plot(df1)
    ax2.plot(df2)
    ax1.set_ylim(bottom=0)
    ax2.set_ylim(bottom=0)
    ax1.title.set_text("Start")
    ax2.title.set_text("End")
    fig.tight_layout()
    plt.savefig("./out/" + filename + ".png", bbox_inches='tight')
