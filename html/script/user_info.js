// 用户信息显示逻辑统一封装

// 检查用户是否登录
function checkUserLogin() {
    // 从localStorage或sessionStorage获取用户信息
    const userInfo = localStorage.getItem('purecpp_user') || sessionStorage.getItem('purecpp_user');
    const token = localStorage.getItem('purecpp_token') || sessionStorage.getItem('purecpp_token');
    
    if (userInfo && token) {
        // 用户已登录，隐藏登录/注册链接，显示用户信息图标
        const loginLink = document.getElementById('login-link');
        const registerLink = document.getElementById('register-link');
        const userInfoElement = document.getElementById('user-info');
        
        if (loginLink) loginLink.style.display = 'none';
        if (registerLink) registerLink.style.display = 'none';
        if (userInfoElement) userInfoElement.style.display = 'block';
        
        // 解析用户信息
        const user = JSON.parse(userInfo);
        
        // 更新用户菜单中的信息
        const usernameElement = document.getElementById('user-menu-username');
        const emailElement = document.getElementById('user-menu-email');
        
        if (usernameElement) usernameElement.textContent = user.username;
        if (emailElement) emailElement.textContent = user.email;
    }
}

// 处理用户菜单的显示和隐藏
function initUserMenu() {
    const userMenuButton = document.getElementById('user-menu-button');
    const userMenu = document.getElementById('user-menu');
    
    if (!userMenuButton || !userMenu) return;
    
    userMenuButton.addEventListener('click', function() {
        userMenu.style.display = userMenu.style.display === 'block' ? 'none' : 'block';
    });
    
    // 点击页面其他地方关闭菜单
    document.addEventListener('click', function(event) {
        if (!userMenuButton.contains(event.target) && !userMenu.contains(event.target)) {
            userMenu.style.display = 'none';
        }
    });
    
    // 处理退出登录
    const logoutLink = document.getElementById('logout-link');
    if (logoutLink) {
        logoutLink.addEventListener('click', function(e) {
            e.preventDefault();
            
            // 清除localStorage和sessionStorage中的用户信息
            localStorage.removeItem('purecpp_user');
            localStorage.removeItem('purecpp_token');
            sessionStorage.removeItem('purecpp_user');
            sessionStorage.removeItem('purecpp_token');
            
            // 刷新页面
            window.location.reload();
        });
    }
}

// 添加用户菜单项的悬停效果
function addUserMenuHoverEffects() {
    const userMenuItems = document.querySelectorAll('.user-menu-item');
    userMenuItems.forEach(item => {
        item.addEventListener('mouseenter', function() {
            this.style.backgroundColor = 'rgba(0, 0, 0, 0.05)';
        });
        item.addEventListener('mouseleave', function() {
            this.style.backgroundColor = 'transparent';
        });
    });
}

// 添加用户菜单按钮的悬停效果
function addUserButtonHoverEffect() {
    const userMenuButton = document.getElementById('user-menu-button');
    if (!userMenuButton) return;
    
    userMenuButton.addEventListener('mouseenter', function() {
        this.style.backgroundColor = 'rgba(0, 0, 0, 0.05)';
    });
    userMenuButton.addEventListener('mouseleave', function() {
        this.style.backgroundColor = 'transparent';
    });
}

// 初始化所有用户信息相关功能
function initUserInfo() {
    checkUserLogin();
    initUserMenu();
    addUserMenuHoverEffects();
    addUserButtonHoverEffect();
}

// 页面加载完成后执行初始化
if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', initUserInfo);
} else {
    // 页面已经加载完成，直接执行
    initUserInfo();
}
