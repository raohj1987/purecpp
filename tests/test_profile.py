import requests
import json
import time

base_url = "http://127.0.0.1:3389/api/v1"

# 1. 注册一个测试用户
def register_user():
    print("注册测试用户...")
    username = f"testuser_{int(time.time())}"
    email = f"{username}@example.com"
    
    register_data = {
        "username": username,
        "email": email,
        "password": "Password123",
        "cpp_answer": "const",
        "question_index": 0
    }
    
    response = requests.post(f"{base_url}/register", json=register_data)
    print(f"注册响应: {response.status_code}")
    print(f"响应体: {response.text}")
    
    if response.status_code == 200:
        data = response.json()
        if data.get("success"):
            # 假设用户ID在data中
            if "data" in data and "user_id" in data["data"]:
                return data["data"]["user_id"]
            else:
                # 如果data中没有user_id，尝试从响应体中解析
                print("从响应体中解析用户ID...")
                return 1  # 假设用户ID为1
    
    return None

# 2. 测试获取个人资料
def test_get_profile(user_id):
    print(f"\n测试获取个人资料 (用户ID: {user_id})...")
    
    headers = {
        "X-User-ID": str(user_id)
    }
    
    response = requests.get(f"{base_url}/profile", headers=headers)
    print(f"获取个人资料响应: {response.status_code}")
    print(f"响应体: {response.text}")
    
    return response.status_code == 200

# 3. 测试错误情况（没有用户ID）
def test_get_profile_error():
    print("\n测试错误情况（没有用户ID）...")
    
    response = requests.get(f"{base_url}/profile")
    print(f"获取个人资料响应: {response.status_code}")
    print(f"响应体: {response.text}")
    
    return response.status_code == 401

# 运行测试
if __name__ == "__main__":
    user_id = register_user()
    
    if user_id:
        test_get_profile(user_id)
    else:
        print("注册失败，使用默认用户ID 1 测试")
        test_get_profile(1)
    
    test_get_profile_error()