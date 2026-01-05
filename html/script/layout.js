// layout.js - 统一布局脚本，包含用户信息管理功能

// DOM加载完成后执行
document.addEventListener('DOMContentLoaded', function() {
    // 初始化主题（在DOM加载完成后立即执行，不等待header和footer加载）
    initTheme();
    
    // 加载页眉
    loadHeader();
    
    // 加载页脚
    loadFooter();
    
    // 初始化通用功能
    initCommonFunctions();
});

// 加载页眉
function loadHeader() {
    fetch('header.html')
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            return response.text();
        })
        .then(html => {
            const headerContainer = document.getElementById('header-container');
            if (headerContainer) {
                headerContainer.innerHTML = html;
                // 确保登录状态的用户图标能正确显示
                if (typeof initUserInfo === 'function') {
                    initUserInfo();
                }
            }
        })
        .catch(error => {
            console.error('Error loading header:', error);
        });
}

// 加载页脚
function loadFooter() {
    fetch('footer.html')
        .then(response => {
            if (!response.ok) {
                throw new Error('Network response was not ok');
            }
            return response.text();
        })
        .then(html => {
            const footerContainer = document.getElementById('footer-container');
            if (footerContainer) {
                footerContainer.innerHTML = html;
                // 页脚加载完成后重新初始化主题切换功能，确保事件绑定正确
                initTheme();
            }
        })
        .catch(error => {
            console.error('Error loading footer:', error);
        });
}

// 初始化通用功能
function initCommonFunctions() {
    // 移动端菜单切换逻辑
    const mobileToggle = document.querySelector('.mobile-toggle');
    if (mobileToggle) {
        mobileToggle.addEventListener('click', toggleMenu);
    }
}

// 主题切换功能
const THEME_STORAGE_KEY = 'purecpp_theme';
const htmlRoot = document.documentElement;

// 初始化主题
function initTheme() {
    const themeToggleBtn = document.getElementById('themeToggleBtn');
    if (themeToggleBtn) {
        // 检查用户是否有手动设置的主题
        const savedTheme = localStorage.getItem(THEME_STORAGE_KEY);
        
        // 如果有手动设置，使用用户设置；否则使用操作系统主题
        const isDarkMode = savedTheme === 'dark' || (!savedTheme && window.matchMedia('(prefers-color-scheme: dark)').matches);
        themeToggleBtn.checked = isDarkMode;
        
        // 确保只应用一个主题类
        htmlRoot.classList.remove('dark', 'light');
        htmlRoot.classList.add(isDarkMode ? 'dark' : 'light');
        
        // 确保事件监听器只绑定一次
        if (!themeToggleBtn.hasAttribute('data-event-bound')) {
            // 绑定主题切换事件
            themeToggleBtn.addEventListener('change', toggleTheme);
            themeToggleBtn.setAttribute('data-event-bound', 'true');
        }
        
        // 确保系统主题变化事件监听器只添加一次
        if (!window.__systemThemeListenerAdded) {
            // 添加监听操作系统主题变化的事件
            if (window.matchMedia) {
                const mediaQuery = window.matchMedia('(prefers-color-scheme: dark)');
                mediaQuery.addEventListener('change', handleSystemThemeChange);
                window.__systemThemeListenerAdded = true;
            }
        }
    }
}

// 处理操作系统主题变化
function handleSystemThemeChange(e) {
    const isDarkMode = e.matches;
    const themeToggleBtn = document.getElementById('themeToggleBtn');
    if (themeToggleBtn) {
        // 无论用户是否有手动设置，都跟随系统主题变化
        themeToggleBtn.checked = isDarkMode;
        
        // 更新主题类
        htmlRoot.classList.remove('dark', 'light');
        htmlRoot.classList.add(isDarkMode ? 'dark' : 'light');
        
        // 更新localStorage中的主题设置
        localStorage.setItem(THEME_STORAGE_KEY, isDarkMode ? 'dark' : 'light');
    }
}

// 切换主题
function toggleTheme() {
    const themeToggleBtn = document.getElementById('themeToggleBtn');
    if (themeToggleBtn) {
        const isDarkMode = themeToggleBtn.checked;
        
        // 确保只应用一个主题类
        htmlRoot.classList.remove('dark', 'light');
        htmlRoot.classList.add(isDarkMode ? 'dark' : 'light');
        
        // 存储用户选择的主题
        localStorage.setItem(THEME_STORAGE_KEY, isDarkMode ? 'dark' : 'light');
    }
}

// 移动端菜单切换
function toggleMenu() {
    const navMenu = document.getElementById('nav-menu');
    if (navMenu) {
        navMenu.classList.toggle('active');
        
        // 切换图标
        const icon = document.querySelector('.mobile-toggle i');
        if (icon) {
            if (navMenu.classList.contains('active')) {
                icon.classList.remove('fa-bars');
                icon.classList.add('fa-times');
            } else {
                icon.classList.remove('fa-times');
                icon.classList.add('fa-bars');
            }
        }
    }
}

// 用户信息显示逻辑统一封装

// 检查用户是否登录
function checkUserLogin() {
    // 从localStorage或sessionStorage获取用户信息
    const userInfo = localStorage.getItem('purecpp_user') || sessionStorage.getItem('purecpp_user');
    const token = localStorage.getItem('purecpp_token') || sessionStorage.getItem('purecpp_token');
    
    // 获取DOM元素
    const loginLink = document.getElementById('login-link');
    const registerLink = document.getElementById('register-link');
    const userInfoElement = document.getElementById('user-info');
    
    if (userInfo && token) {
        // 用户已登录，隐藏登录/注册链接，显示用户信息图标
        if (loginLink) loginLink.style.display = 'none';
        if (registerLink) registerLink.style.display = 'none';
        if (userInfoElement) userInfoElement.style.display = 'block';
        
        // 解析用户信息
        try {
            const user = JSON.parse(userInfo);
            
            // 更新用户菜单中的信息
            const usernameElement = document.getElementById('user-menu-username');

            if (usernameElement) usernameElement.textContent = "你好! " + user.username;
        } catch (error) {
            console.error('Error parsing user info:', error);
        }
    } else {
        // 用户未登录，显示登录/注册链接，隐藏用户信息图标
        if (loginLink) loginLink.style.display = 'block';
        if (registerLink) registerLink.style.display = 'block';
        if (userInfoElement) userInfoElement.style.display = 'none';
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
        logoutLink.addEventListener('click', async function (e) {
            e.preventDefault();

            // 获取当前用户的token和用户信息
            const token = localStorage.getItem('purecpp_token') || sessionStorage.getItem('purecpp_token');
            const userInfoStr = localStorage.getItem('purecpp_user') || sessionStorage.getItem('purecpp_user');

            // 提取用户ID
            let userId = null;
            if (userInfoStr) {
                try {
                    const userInfo = JSON.parse(userInfoStr);
                    userId = userInfo.id || userInfo.user_id;
                } catch (error) {
                    console.error('Error parsing user info:', error);
                }
            }

            // 构造请求头和请求体
            const headers = {'Content-Type': 'application/json'};
            const body = {user_id: userId};

            if (token) {
                headers['Authorization'] = `Bearer ${token}`;
            }

            // 调用后台logout服务
            try {
                await fetch('/api/v1/logout', {
                    method: 'POST',
                    headers: headers,
                    body: JSON.stringify(body)
                });
            } catch (error) {
                console.error('Logout request failed:', error);
            } finally {
                // 清除localStorage和sessionStorage中的用户信息
                localStorage.removeItem('purecpp_user');
                localStorage.removeItem('purecpp_token');
                sessionStorage.removeItem('purecpp_user');
                sessionStorage.removeItem('purecpp_token');

                // 刷新页面
                window.location.reload();
            }
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