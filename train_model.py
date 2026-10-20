import pandas as pd
import numpy as np
from sklearn.model_selection import train_test_split
from sklearn.ensemble import RandomForestClassifier
from sklearn.metrics import classification_report, accuracy_score
import joblib

# 1. Load Simulation Telemetry Dataset
df = pd.read_csv("/home/ghari/ns-3.48/metrics_traditional.csv")

print("--- Telemetry Data Sample ---")
print(df.head())

# 2. Feature Selection
features = ['tx_packets', 'rx_packets', 'delay_ms', 'throughput_mbps', 'loss_ratio']
X = df[features]
y = df['is_congested']

# Augment samples for robust training boundary
X_noisy = X.copy() + np.random.normal(0, 0.02, X.shape)
y_noisy = y.copy()

X_all = pd.concat([X, X_noisy], ignore_index=True)
y_all = pd.concat([y, y_noisy], ignore_index=True)

X_train, X_test, y_train, y_test = train_test_split(X_all, y_all, test_size=0.3, random_state=42)

# 3. Model Training
clf = RandomForestClassifier(n_estimators=50, max_depth=5, random_state=42)
clf.fit(X_train, y_train)

# 4. Evaluation
preds = clf.predict(X_test)
print("\n--- Model Evaluation ---")
print(f"Accuracy: {accuracy_score(y_test, preds) * 100:.2f}%")
print(classification_report(y_test, preds))

# 5. Export Model
joblib.dump(clf, "congestion_model.pkl")
print("Saved trained model to congestion_model.pkl")
