// api_service.js - 集中处理所有后端API请求，包含token自动刷新功能

class APIService {
    constructor() {
        this.baseURL = '';
        this.isRefreshing = false;
        this.refreshTokenQueue = [];
        this.tokenKey = 'purecpp_access_token';
        this.refreshTokenKey = 'purecpp_refresh_token';
        this.accessTokenExpiresKey = 'purecpp_access_token_expires_at';
        this.refreshTokenExpiresKey = 'purecpp_refresh_token_expires_at';
        this.userInfoKey = 'purecpp_user';
        this.serverTimeOffset = 0; // 服务器时间偏移量（ms）
        this.maxRefreshAttempts = 3; // 最大刷新尝试次数
        this.refreshAttempts = 0; // 当前刷新尝试次数
        this.lastRefreshTime = 0; // 上次刷新时间
    }

    // 获取存储的token
    getAccessToken() {
        return sessionStorage.getItem(this.tokenKey);
    }

    getRefreshToken() {
        return localStorage.getItem(this.refreshTokenKey);
    }

    getAccessTokenExpiresAt() {
        const expiresAt = sessionStorage.getItem(this.accessTokenExpiresKey);
        return expiresAt ? parseInt(expiresAt) : 0;
    }

    // 存储token
    // access_token存储到sessionStorage，refresh_token存储到localStorage
    saveTokens(accessToken, refreshToken, accessTokenExpiresAt, refreshTokenExpiresAt, rememberMe = false) {
        // access_token存储到sessionStorage
        sessionStorage.setItem(this.tokenKey, accessToken);
        sessionStorage.setItem(this.accessTokenExpiresKey, accessTokenExpiresAt.toString());

        // refresh_token存储到localStorage
        localStorage.setItem(this.refreshTokenKey, refreshToken);
        localStorage.setItem(this.refreshTokenExpiresKey, refreshTokenExpiresAt.toString());
    }

    // 清除所有token
    clearTokens() {
        localStorage.removeItem(this.tokenKey);
        localStorage.removeItem(this.refreshTokenKey);
        localStorage.removeItem(this.accessTokenExpiresKey);
        localStorage.removeItem(this.refreshTokenExpiresKey);
        localStorage.removeItem(this.userInfoKey);

        sessionStorage.removeItem(this.tokenKey);
        sessionStorage.removeItem(this.accessTokenExpiresKey);
        sessionStorage.removeItem(this.userInfoKey);
    }

    // 保存用户信息
    saveUserInfo(userInfo) {
        const tokenStorage = localStorage.getItem(this.tokenKey) ? localStorage : sessionStorage;
        tokenStorage.setItem(this.userInfoKey, JSON.stringify(userInfo));
    }

    // 获取用户信息
    getUserInfo() {
        const userInfo = localStorage.getItem(this.userInfoKey) || sessionStorage.getItem(this.userInfoKey);
        return userInfo ? JSON.parse(userInfo) : null;
    }

    // 检查token是否即将过期
    isAccessTokenExpiring() {
        // Date.now()返回毫秒级时间戳，需要转换为秒级时间戳
        const now = Math.floor(Date.now() / 1000) + this.serverTimeOffset; // 考虑服务器时间偏移，转换为秒级
        const expiresAt = this.getAccessTokenExpiresAt(); // 后台返回的是秒级时间戳
        if (!expiresAt) return false;

        const tokenLifetime = expiresAt - now;
        if (tokenLifetime <= 0) return true;

        // 动态调整提前刷新时间：最多提前30秒，或有效期的10%，取较小值
        const refreshThreshold = Math.min(30, tokenLifetime * 0.1); // 现在都是秒级，所以30秒直接写30
        return now > expiresAt - refreshThreshold;
    }

    // 刷新token
    async refreshToken() {
        const refreshToken = this.getRefreshToken();

        if (!refreshToken) {
            throw new Error('No refresh token available');
        }

        // 检查是否过于频繁刷新
        const now = Date.now();
        if (now - this.lastRefreshTime < 1000 && this.refreshAttempts > 0) {
            // 避免1秒内多次刷新
            throw new Error('Refresh token too frequently');
        }

        // 检查最大尝试次数
        if (this.refreshAttempts >= this.maxRefreshAttempts) {
            this.clearTokens();
            window.location.href = '/login.html';
            throw new Error('Max refresh attempts reached');
        }

        try {
            this.refreshAttempts++;
            this.lastRefreshTime = now;

            // 获取用户信息
            const userInfo = this.getUserInfo();
            const userId = userInfo ? userInfo.id : null;

            const response = await fetch(`${this.baseURL}/api/v1/refresh_token`, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    refresh_token: refreshToken,
                    user_id: userId
                })
            });

            const data = await response.json();

            if (!data.success) {
                // 区分刷新token过期和其他错误
                if (data.code === 401 || data.message.includes('refresh token')) {
                    // 刷新token过期或无效
                    this.clearTokens();
                    window.location.href = '/login.html';
                }
                throw new Error(data.message || 'Failed to refresh token');
            }

            // 保存新的tokens
            const {
                token,
                refresh_token,
                access_token_expires_at,
                refresh_token_expires_at,
                access_token_lifetime = 3600
            } = data.data;

            // 计算服务器时间偏移量：使用服务器返回的实际有效期，默认3600秒
            // 服务器当前时间 = access_token_expires_at - access_token_lifetime
            const serverCurrentTime = access_token_expires_at - access_token_lifetime; // 秒级
            const clientCurrentTime = Math.floor(Date.now() / 1000); // 转换为秒级
            this.serverTimeOffset = serverCurrentTime - clientCurrentTime; // 秒级偏移量

            this.saveTokens(token, refresh_token, access_token_expires_at, refresh_token_expires_at);

            // 重置刷新尝试次数
            this.refreshAttempts = 0;

            // 处理刷新队列
            this.refreshTokenQueue.forEach(cb => cb(token));
            this.refreshTokenQueue = [];

            return token;
        } catch (error) {
            // 指数退避：如果还有尝试次数，等待一段时间后重试
            if (this.refreshAttempts < this.maxRefreshAttempts) {
                const delay = Math.pow(2, this.refreshAttempts) * 1000;
                await new Promise(resolve => setTimeout(resolve, delay));
                return this.refreshToken(); // 递归重试
            } else {
                // 刷新失败，清除所有tokens并跳转到登录页
                this.clearTokens();
                window.location.href = '/login.html';
                throw error;
            }
        } finally {
            this.isRefreshing = false;
        }
    }

    // 统一处理请求
    async request(endpoint, options = {}) {
        const url = `${this.baseURL}${endpoint}`;
        const headers = {
            'Content-Type': 'application/json',
            ...options.headers
        };

        // 检查token是否即将过期
        if (this.isAccessTokenExpiring()) {
            if (!this.isRefreshing) {
                this.isRefreshing = true;
                try {
                    const newToken = await this.refreshToken();
                    headers['Authorization'] = `Bearer ${newToken}`;
                } catch (error) {
                    throw error;
                }
            } else {
                // 等待token刷新完成
                return new Promise((resolve, reject) => {
                    this.refreshTokenQueue.push((newToken) => {
                        headers['Authorization'] = `Bearer ${newToken}`;
                        this._fetch(url, {...options, headers}).then(resolve).catch(reject);
                    });
                });
            }
        } else {
            // 添加现有的token
            const accessToken = this.getAccessToken();
            if (accessToken) {
                headers['Authorization'] = `Bearer ${accessToken}`;
            }
        }

        return this._fetch(url, {...options, headers});
    }

    // 实际执行fetch请求
    async _fetch(url, options) {
        try {
            const response = await fetch(url, options);
            const data = await response.json();

            if (!response.ok) {
                // 处理401未授权错误
                if (response.status === 401 && !options._retry) {
                    // 尝试刷新token并重新请求
                    if (!this.isRefreshing) {
                        this.isRefreshing = true;
                        try {
                            const newToken = await this.refreshToken();
                            options.headers['Authorization'] = `Bearer ${newToken}`;
                            options._retry = true;
                            return this._fetch(url, options);
                        } catch (error) {
                            throw error;
                        }
                    } else {
                        // 等待token刷新完成
                        return new Promise((resolve, reject) => {
                            this.refreshTokenQueue.push((newToken) => {
                                options.headers['Authorization'] = `Bearer ${newToken}`;
                                options._retry = true;
                                this._fetch(url, options).then(resolve).catch(reject);
                            });
                        });
                    }
                }

                // 其他错误
                throw new Error(data.message || 'Request failed');
            }

            return data;
        } catch (error) {
            throw error;
        }
    }

    // API方法封装

    // 用户登录
    async login(username, password, rememberMe = false) {
        const data = await this.request('/api/v1/login', {
            method: 'POST',
            body: JSON.stringify({username, password})
        });

        // 保存token和用户信息
        if (data.success && data.data) {
            const {
                token,
                refresh_token,
                access_token_expires_at,
                refresh_token_expires_at,
                access_token_lifetime = 3600,
                user_id,
                username: user_name,
                email,
                title,
                role,
                avatar = '',
                experience,
                level
            } = data.data;

            // 计算服务器时间偏移量：使用服务器返回的实际有效期，默认3600秒
            // 服务器当前时间 = access_token_expires_at - access_token_lifetime
            const serverCurrentTime = access_token_expires_at - access_token_lifetime; // 秒级
            const clientCurrentTime = Math.floor(Date.now() / 1000); // 转换为秒级
            this.serverTimeOffset = serverCurrentTime - clientCurrentTime; // 秒级偏移量

            this.saveTokens(token, refresh_token, access_token_expires_at, refresh_token_expires_at, rememberMe);

            // 保存用户信息
            this.saveUserInfo({
                id: user_id,
                username: user_name,
                email,
                title,
                role,
                experience,
                level,
                avatar: avatar || ''
            });
        }

        return data;
    }

    // 用户登出
    async logout() {
        const userInfo = this.getUserInfo();
        const userId = userInfo ? userInfo.id : null;

        try {
            await this.request('/api/v1/logout', {
                method: 'POST',
                body: JSON.stringify({user_id: userId})
            });
        } catch (error) {
            console.error('Logout failed:', error);
        } finally {
            this.clearTokens();
        }
    }

    // 注册新用户
    async register(username, email, password, cppAnswer, questionIndex) {
        return this.request('/api/v1/register', {
            method: 'POST',
            body: JSON.stringify({username, email, password, cpp_answer: cppAnswer, question_index: questionIndex})
        });
    }

    // 获取注册问题
    async getQuestions() {
        return this.request('/api/v1/get_questions', {
            method: 'GET'
        });
    }

    // 邮箱验证
    async verifyEmail(token) {
        return this.request('/api/v1/verify_email', {
            method: 'POST',
            body: JSON.stringify({token})
        });
    }

    // 重新发送验证邮件
    async resendVerifyEmail(email) {
        return this.request('/api/v1/resend_verify_email', {
            method: 'POST',
            body: JSON.stringify({email})
        });
    }

    // 忘记密码
    async forgotPassword(email) {
        return this.request('/api/v1/forgot_password', {
            method: 'POST',
            body: JSON.stringify({email})
        });
    }

    // 重置密码
    async resetPassword(token, newPassword) {
        return this.request('/api/v1/reset_password', {
            method: 'POST',
            body: JSON.stringify({token, new_password: newPassword})
        });
    }

    // 修改密码
    async changePassword(userId, oldPassword, newPassword) {
        return this.request('/api/v1/change_password', {
            method: 'POST',
            body: JSON.stringify({user_id: userId, old_password: oldPassword, new_password: newPassword})
        });
    }

    // 获取文章列表
    async getArticles() {
        return this.request('/api/v1/get_articles', {
            method: 'GET'
        });
    }

    // 获取文章数量
    async getArticleCount() {
        return this.request('/api/v1/get_article_count', {
            method: 'GET'
        });
    }

    // 获取单个文章
    async getArticle(slug) {
        return this.request(`/api/v1/article/${slug}`, {
            method: 'GET'
        });
    }

    // 创建新文章
    async createArticle(title, excerpt, content, tagId) {
        return this.request('/api/v1/new_article', {
            method: 'POST',
            body: JSON.stringify({title, excerpt, content, tag_id: tagId})
        });
    }

    // 编辑文章
    async editArticle(slug, title, excerpt, content, tagId, username) {
        return this.request('/api/v1/edit_article', {
            method: 'POST',
            body: JSON.stringify({slug, title, excerpt, content, tag_id: tagId, username})
        });
    }

    // 获取待审核文章
    async getPendingArticles() {
        return this.request('/api/v1/get_pending_articles', {
            method: 'GET'
        });
    }

    // 审核文章
    async reviewArticle(reviewer_name, slug, review_status) {
        return this.request('/api/v1/review_pending_article', {
            method: 'POST',
            body: JSON.stringify({reviewer_name, slug, review_status})
        });
    }

    // 获取标签列表
    async getTags() {
        return this.request('/api/v1/get_tags', {
            method: 'GET'
        });
    }

    // 获取文章评论
    async getArticleComments(slug) {
        return this.request(`/api/v1/get_article_comment/${slug}`, {
            method: 'GET'
        });
    }

    // 添加文章评论
    async addArticleComment(slug, content, parentId = 0) {
        const userInfo = this.getUserInfo();
        if (!userInfo) {
            throw new Error('User not logged in');
        }

        return this.request('/api/v1/add_article_comment', {
            method: 'POST',
            body: JSON.stringify({
                content,
                parent_comment_id: parentId,
                author_name: userInfo.username,
                slug
            })
        });
    }

    // 获取用户个人资料，支持通过userId或username查询
    async getUserProfile(identifier) {
        const payload = typeof identifier === 'string'
            ? {username: identifier}
            : {user_id: identifier};

        return this.request('/api/v1/user/get_profile', {
            method: 'POST',
            body: JSON.stringify(payload)
        });
    }

    // 更新用户个人资料
    async updateUserProfile(profileData) {
        const response = await this.request('/api/v1/user/update_profile', {
            method: 'POST',
            body: JSON.stringify(profileData)
        });

        // 如果更新成功，更新本地存储的用户信息
        if (response.success) {
            const userInfo = this.getUserInfo();
            if (userInfo) {
                // 更新用户头像（如果返回了头像信息）
                if (response.data && response.data.avatar) {
                    userInfo.avatar = response.data.avatar;
                    this.saveUserInfo(userInfo);
                }
            }
        }

        return response;
    }

    // 上传用户头像
    async uploadAvatar(userId, file) {
        // 将图片转换为base64格式的辅助方法
        const fileToBase64 = (file) => {
            return new Promise((resolve, reject) => {
                const reader = new FileReader();
                reader.onload = () => resolve(reader.result.split(',')[1]);
                reader.onerror = error => reject(error);
                reader.readAsDataURL(file);
            });
        };

        try {
            // 将图片转换为base64格式
            const base64Data = await fileToBase64(file);

            // 构建JSON请求
            const uploadData = {
                user_id: userId,
                avatar_data: base64Data,
                filename: file.name
            };

            const response = await this.request('/api/v1/user/upload_avatar', {
                method: 'POST',
                body: JSON.stringify(uploadData)
            });

            // 如果上传成功，更新本地存储的用户头像信息
            if (response.success && response.data && response.data.url) {
                const userInfo = this.getUserInfo();
                if (userInfo) {
                    userInfo.avatar = response.data.url;
                    this.saveUserInfo(userInfo);
                }
            }

            return response;
        } catch (error) {
            throw error;
        }
    }
}

// 创建单例实例
const apiService = new APIService();
