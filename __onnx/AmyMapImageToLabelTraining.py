import torch
import torch.nn as nn
import torch.optim as optim
import cv2
import numpy as np
from torch.utils.data import DataLoader, Dataset

##########
# Mostly coded by Google Gemini. I don't have enough knowledge on machine learning.
# - DJSong Feb 2026
##########

# How wide surrounding pixel area to be referred for a single image pixel. Changing it also requires change the matching constant in Unreal side.
PATCH_SIZE = 63

# How many pixels get picked for training among whole image. It directly affects training time. 
TRAINING_SAMPLE_NUM=300000

MAP_FILE = 'JejuMap_Kakao_Training_01.png'  
LABEL_FILE = 'JejuMap_Kakao_Training_01_Label.png'     # The file that manually labeled by your intention
MODEL_NAME = "AmyMapImageToLabelModel.onnx" # The output file

# 1. Defining model
class ShapeAwareNet(nn.Module):
    def __init__(self, patch_size=31):
        super(ShapeAwareNet, self).__init__()
        self.features = nn.Sequential(
            nn.Conv2d(3, 32, kernel_size=3, padding=1),
            nn.ReLU(),
            nn.MaxPool2d(2), 
            nn.Conv2d(32, 64, kernel_size=3, padding=1),
            nn.ReLU(),
            nn.MaxPool2d(2),
            nn.Flatten()
        )
        # (N // 4) is the approx value gone through MaxPool(2) twice
        # In the case of 31x31, MaxPool twice, then 7x7 (64 channel * 7 * 7 = 3136)
        reduced_size = patch_size // 4
        self.classifier = nn.Sequential(
            nn.Linear(64 * reduced_size * reduced_size, 128),
            nn.ReLU(),
            nn.Linear(128, 3) # [R, G, B]
        )

    def forward(self, x):
        x = self.features(x)
        return self.classifier(x)

# 2. Dataset class (Extrating image patch)
class MapPairDataset(Dataset):
    def __init__(self, map_path, label_path, patch_size=31, num_samples=200000):
        self.map_img = cv2.cvtColor(cv2.imread(map_path), cv2.COLOR_BGR2RGB) / 255.0
        self.label_img = cv2.cvtColor(cv2.imread(label_path), cv2.COLOR_BGR2RGB)
        self.patch_size = patch_size
        self.half_p = patch_size // 2
        self.h, self.w, _ = self.map_img.shape
        self.coords = [(np.random.randint(self.half_p, self.w - self.half_p), 
                        np.random.randint(self.half_p, self.h - self.half_p)) for _ in range(num_samples)]

    def __len__(self): return len(self.coords)

    def __getitem__(self, idx):
        x, y = self.coords[idx]
        patch = self.map_img[y-self.half_p : y+self.half_p+1, x-self.half_p : x+self.half_p+1]
        label_pixel = self.label_img[y, x]
        
        # Just normalizing in 0.0 ~ 1.0 range
        target_rgb = label_pixel / 255.0
            
        return torch.tensor(patch.transpose(2,0,1), dtype=torch.float32), torch.tensor(target_rgb, dtype=torch.float32)
       
# 3. The main execution 
if __name__ == "__main__":
    
    print("Loading data...")
    dataset = MapPairDataset(MAP_FILE, LABEL_FILE, patch_size=PATCH_SIZE, num_samples=TRAINING_SAMPLE_NUM)
    dataloader = DataLoader(dataset, batch_size=64, shuffle=True)

    # Model and optimization settings
    model = ShapeAwareNet(patch_size=PATCH_SIZE)
    optimizer = optim.Adam(model.parameters(), lr=0.001)
    criterion = nn.MSELoss()

    # Training loop
    print("Begin training .. (Suggest 10~20 epoch )...")
    model.train()
    for epoch in range(15): # It is just for fun, so 10 ~ 20 should be enough
        running_loss = 0.0
        for inputs, targets in dataloader:
            optimizer.zero_grad()
            outputs = model(inputs)
            loss = criterion(outputs, targets)
            loss.backward()
            optimizer.step()
            running_loss += loss.item()
        
        print(f"Epoch {epoch+1}/15 - Loss: {running_loss/len(dataloader):.4f}")

    # 4. Saving ONNX 
    model.eval()
    dummy_input = torch.randn(1, 3, PATCH_SIZE, PATCH_SIZE) # Unreal NNE should reflect this size 
    torch.onnx.export(model, dummy_input, MODEL_NAME, 
                      input_names=['input'], output_names=['output'],
                      opset_version=11)
    
    print(f"Congratulation! {MODEL_NAME} file has been generated!")