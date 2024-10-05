import matplotlib.pyplot as plt
import numpy as np

data = np.loadtxt("Measure.txt")
angle = data[:, 0];
FPSs = data[:, 1];
FPSs = 1000.0 / FPSs

avg = np.mean(FPSs)
maxx = np.max(FPSs)
minn = np.min(FPSs)
print(f"mean: {avg:0.0f} max: {maxx:0.0f} min: {minn:0.0f} FPS")

plt.plot(angle, FPSs)
plt.xlabel("angle")
plt.ylabel("FPS")
plt.show()
