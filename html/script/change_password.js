// 修改密码功能实现

// 表单验证函数
function validateForm() {
    let isValid = true;
    
    // 重置错误信息
    document.getElementById('oldPasswordError').textContent = '';
    document.getElementById('newPasswordError').textContent = '';
    document.getElementById('confirmPasswordError').textContent = '';
    
    // 获取表单值
    const oldPassword = document.getElementById('oldPassword').value;
    const newPassword = document.getElementById('newPassword').value;
    const confirmPassword = document.getElementById('confirmPassword').value;
    
    // 验证旧密码
    if (!oldPassword) {
        document.getElementById('oldPasswordError').textContent = '请输入旧密码';
        isValid = false;
    }
    
    // 验证新密码
    if (!newPassword) {
        document.getElementById('newPasswordError').textContent = '请输入新密码';
        isValid = false;
    } else {
        // 检查新密码长度
        if (newPassword.length < 6 || newPassword.length > 20) {
            document.getElementById('newPasswordError').textContent = '密码长度必须在6-20位之间';
            isValid = false;
        }
        
        // 检查新密码是否包含大小写字母和数字
        const hasUpper = /[A-Z]/.test(newPassword);
        const hasLower = /[a-z]/.test(newPassword);
        const hasDigit = /\d/.test(newPassword);
        
        if (!hasUpper || !hasLower || !hasDigit) {
            document.getElementById('newPasswordError').textContent = '密码必须包含大小写字母和数字';
            isValid = false;
        }
        
        // 检查新密码是否与旧密码相同
        if (newPassword === oldPassword) {
            document.getElementById('newPasswordError').textContent = '新密码不能与旧密码相同';
            isValid = false;
        }
    }
    
    // 验证确认密码
    if (!confirmPassword) {
        document.getElementById('confirmPasswordError').textContent = '请确认新密码';
        isValid = false;
    } else {
        if (newPassword !== confirmPassword) {
            document.getElementById('confirmPasswordError').textContent = '两次输入的新密码不一致';
            isValid = false;
        }
    }
    
    return isValid;
}

// 显示消息函数
function showMessage(message, isError = false) {
    const messageElement = document.getElementById('message');
    messageElement.textContent = message;
    messageElement.style.color = isError ? '#ef4444' : '#10b981';
}

// 修改密码提交处理
async function handleChangePassword(event) {
    event.preventDefault();
    
    // 表单验证
    if (!validateForm()) {
        return;
    }
    
    // 获取用户信息 - 同时检查localStorage和sessionStorage
    const userInfoStr = localStorage.getItem('purecpp_user') || sessionStorage.getItem('purecpp_user');
    if (!userInfoStr) {
        showMessage('请先登录', true);
        setTimeout(() => {
            window.location.href = 'login.html';
        }, 1500);
        return;
    }
    
    const userInfo = JSON.parse(userInfoStr);
    const userId = userInfo.user_id || userInfo.id;
    
    if (!userId) {
        showMessage('用户信息不完整，请重新登录', true);
        setTimeout(() => {
            window.location.href = 'login.html';
        }, 1500);
        return;
    }
    
    // 获取表单值
    const oldPassword = document.getElementById('oldPassword').value;
    const newPassword = document.getElementById('newPassword').value;
    
    // 构建请求数据
    const requestData = {
        user_id: userId,
        old_password: oldPassword,
        new_password: newPassword
    };
    
    // 禁用提交按钮
    const submitButton = document.getElementById('submitButton');
    submitButton.disabled = true;
    submitButton.textContent = '修改中...';
    
    try {
        // 获取token
        const token = localStorage.getItem('purecpp_token') || sessionStorage.getItem('purecpp_token');
        
        // 发送请求
        const response = await fetch('/api/v1/change_password', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Authorization': `Bearer ${token}`
            },
            body: JSON.stringify(requestData)
        });
        
        // 解析响应
        const result = await response.json();
        
        if (result.success) {
            showMessage('密码修改成功！');
            
            // 清除表单
            document.getElementById('changePasswordForm').reset();
            
            // 3秒后跳转到首页
            setTimeout(() => {
                window.location.href = 'index.html';
            }, 3000);
        } else {
            showMessage(result.message || '密码修改失败', true);
        }
    } catch (error) {
        console.error('修改密码失败:', error);
        showMessage('网络错误，请稍后重试', true);
    } finally {
        // 启用提交按钮
        submitButton.disabled = false;
        submitButton.textContent = '修改密码';
    }
}

// 页面加载完成后初始化
window.addEventListener('DOMContentLoaded', () => {
    // 绑定表单提交事件
    const changePasswordForm = document.getElementById('changePasswordForm');
    changePasswordForm.addEventListener('submit', handleChangePassword);
    
    // 检查用户是否已登录 - 同时检查localStorage和sessionStorage
    const userInfo = localStorage.getItem('purecpp_user') || sessionStorage.getItem('purecpp_user');
    const token = localStorage.getItem('purecpp_token') || sessionStorage.getItem('purecpp_token');
    
    if (!userInfo || !token) {
        showMessage('请先登录', true);
        setTimeout(() => {
            window.location.href = 'login.html';
        }, 1500);
    }
});