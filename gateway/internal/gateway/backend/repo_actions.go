package backend

// GetRepo returns the access configuration of a repository
func (s *Services) GetRepo(repoName string) *RepositoryConfig {
	return s.Access.GetRepo(repoName)
}

// GetRepos returns a map with repository access configurations
func (s *Services) GetRepos() map[string]RepositoryConfig {
	return s.Access.GetRepos()
}
